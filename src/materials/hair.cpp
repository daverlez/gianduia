#include <gianduia/materials/hair.h>
#include <gianduia/materials/bsdf.h>
#include <gianduia/materials/material.h>
#include <gianduia/materials/fresnel.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/constants.h>
#include <gianduia/shapes/shape.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numeric>

namespace gnd {

    // Anonymous namespace utilities

    namespace {
        inline float Sqr(float x) { return x * x; }
        inline float SafeSqrt(float x) { return std::sqrt(std::max(0.f, x)); }

        inline Color3f ColorExp(const Color3f &c) {
            return Color3f(std::exp(c.r()), std::exp(c.g()), std::exp(c.b()));
        }

        // Fast polynomial approximation for Bessel function
        float I0(float x) {
            float ax = std::abs(x);
            if (ax < 3.75f) {
                float y = x / 3.75f;
                y *= y;
                return 1.0f + y * (3.5156229f + y * (3.0899424f + y * (1.2067492f
                            + y * (0.2659732f + y * (0.0360768f + y * 0.0045813f)))));
            }
            float y = 3.75f / ax;
            return (std::exp(ax) / std::sqrt(ax)) * (0.39894228f + y * (0.01328592f
                 + y * (0.00225319f + y * (-0.00157565f + y * (0.00916281f
                 + y * (-0.02057706f + y * (0.02635537f + y * (-0.01647633f
                 + y * 0.00392377f))))))));
        }

        float LogI0(float x) {
            if (x > 12.f)
                return x + 0.5f * (-std::log(TwoPi) + std::log(1.f / x) + 1.f / (8.f * x));

            return std::log(I0(x));
        }

        //  Longitudinal scattering M_p

        float Mp(float cosThetaI, float cosThetaO,
                 float sinThetaI, float sinThetaO,
                 float v) {
            float a = cosThetaI * cosThetaO / v;
            float b = sinThetaI * sinThetaO / v;
            if (v <= 0.1f)
                return std::exp(LogI0(a) - b - 1.f / v + 0.6931f + std::log(0.5f / v));
            else
                return std::exp(-b) * I0(a) / (2.f * v * std::sinh(1.f / v));
        }

        //  Azimuthal scattering  N_p  (trimmed logistic distribution on a circle)

        float Logistic(float x, float s) {
            x = std::abs(x);
            float ex = std::exp(-x / s);
            return ex / (s * Sqr(1.f + ex));
        }

        float LogisticCDF(float x, float s) {
            return 1.f / (1.f + std::exp(-x / s));
        }

        float TrimmedLogistic(float x, float s, float a, float b) {
            return Logistic(x, s) / (LogisticCDF(b, s) - LogisticCDF(a, s));
        }

        float SampleTrimmedLogistic(float u, float s, float a, float b) {
            float k = LogisticCDF(b, s) - LogisticCDF(a, s);

            float val = u * k + LogisticCDF(a, s);
            val = std::clamp(val, 1e-5f, 1.f - 1e-5f);

            float x = -s * std::log(1.f / val - 1.f);
            return std::clamp(x, a, b);
        }

        float Phi(int p, float gammaO, float gammaT) {
            return 2.f * static_cast<float>(p) * gammaT - 2.f * gammaO
                   + static_cast<float>(p) * Pi;
        }

        float Np(float phi, int p, float s, float gammaO, float gammaT) {
            float dphi = phi - Phi(p, gammaO, gammaT);
            // Wrap to (−π, π]
            while (dphi > Pi) dphi -= TwoPi;
            while (dphi < -Pi) dphi += TwoPi;
            return TrimmedLogistic(dphi, s, -Pi, Pi);
        }
    }


    //  Public utility implementations

    Color3f SigmaAFromConcentration(float eumelanin, float pheomelanin) {
        static const float eu[3] = {0.419f, 0.697f, 1.370f};
        static const float ph[3] = {0.187f, 0.400f, 1.050f};
        return Color3f(eumelanin * eu[0] + pheomelanin * ph[0],
                       eumelanin * eu[1] + pheomelanin * ph[1],
                       eumelanin * eu[2] + pheomelanin * ph[2]);
    }

    Color3f SigmaAFromReflectance(const Color3f &c, float beta_n) {
        auto invert = [&](float ci) -> float {
            float lnc = std::log(std::max(ci, 1e-4f));
            float b = beta_n;
            float d = 5.969f
                      - 0.215f * b
                      + 2.532f * Sqr(b)
                      - 10.73f * b * Sqr(b)
                      + 5.574f * Sqr(Sqr(b))
                      + 0.245f * b * Sqr(Sqr(b));
            return Sqr(lnc / d);
        };
        return Color3f(invert(c.r()), invert(c.g()), invert(c.b()));
    }


    //  HairBxDF

    HairBxDF::HairBxDF(float h, float eta, const Color3f &sigma_a,
                       float beta_m, float beta_n, float alpha)
        : BxDF(BxDFType(BSDF_GLOSSY | BSDF_REFLECTION | BSDF_TRANSMISSION)),
          m_h(h),
          m_gammaO(std::asin(std::clamp(h, -1.f, 1.f))),
          m_eta(eta),
          m_sigma_a(sigma_a),
          m_beta_m(std::max(1e-3f, beta_m)),
          m_beta_n(std::max(1e-3f, beta_n)),
          m_alpha(alpha) {

        // Longitudinal variances (empirical fit from d'Eon 2011)
        // v[0] = R,  v[1] = TT (tighter),  v[2] = TRT (broader),  v[3+] = residual
        m_v[0] = Sqr(0.726f * beta_m + 0.812f * Sqr(beta_m) + 3.7f * std::pow(beta_m, 20.f));
        m_v[1] = 0.25f * m_v[0];
        m_v[2] = 4.f * m_v[0];
        for (int p = 3; p <= pMax; ++p)
            m_v[p] = m_v[2];

        // Azimuthal logistic scale (empirical fit from d'Eon 2011)
        m_s = SqrtPiOver8
              * (0.265f * beta_n + 1.194f * Sqr(beta_n) + 5.372f * std::pow(beta_n, 22.f));

        // sin / cos of 2^k * alpha for the cuticle-tilt shifts (k = 0, 1, 2)
        float alphaRad = alpha * Pi / 180.f;
        m_sin2kAlpha[0] = std::sin(alphaRad);
        m_cos2kAlpha[0] = std::cos(alphaRad);
        for (int k = 1; k < 3; ++k) {
            m_sin2kAlpha[k] = 2.f * m_cos2kAlpha[k - 1] * m_sin2kAlpha[k - 1];
            m_cos2kAlpha[k] = Sqr(m_cos2kAlpha[k - 1]) - Sqr(m_sin2kAlpha[k - 1]);
        }
    }


    //  Private helpers

    std::array<Color3f, pMax + 1> HairBxDF::computeAp(float cosThetaO, float cosGammaO, float cosGammaT, float cosThetaT) const {
        std::array<Color3f, pMax + 1> ap;

        float cosTheta = cosThetaO * cosGammaO;
        float f = FrDielectric(cosTheta, 1.f, m_eta);

        float safeCosThetaT = std::max(cosThetaT, 1e-5f);
        Color3f T = ColorExp(m_sigma_a * (-2.f * cosGammaT / safeCosThetaT));

        ap[0] = Color3f(f);
        ap[1] = Sqr(1.f - f) * T;

        for (int p = 2; p < pMax; ++p)
            ap[p] = ap[p - 1] * f * T;

        for (int ch = 0; ch < 3; ++ch) {
            float ft = std::min(f * T[ch], 0.999f);
            float denom = 1.f - ft;
            ap[pMax][ch] = ap[pMax - 1][ch] * ft / denom;
        }

        return ap;
    }

    std::array<float, pMax + 1> HairBxDF::computeApPdf(const std::array<Color3f, pMax + 1> &ap) const {
        std::array<float, pMax + 1> weights;
        float sum = 0.f;
        for (int p = 0; p <= pMax; ++p) {
            weights[p] = ap[p].luminance();
            sum += weights[p];
        }
        if (sum > 0.f)
            for (int p = 0; p <= pMax; ++p) weights[p] /= sum;
        else
            for (int p = 0; p <= pMax; ++p) weights[p] = 1.f / (pMax + 1);
        return weights;
    }

    void HairBxDF::tiltedAngles(int p, float sinThetaO, float cosThetaO,
                                float &sinThetaOp, float &cosThetaOp) const {
        // The cuticle scales are tilted by alpha away from the surface normal.
        // This shifts the effective longitudinal angle differently for each lobe:
        //   R   (p=0):  −2alpha  (reflection off tilted outer cuticle)
        //   TT  (p=1):  + alpha  (refraction through the cuticle)
        //   TRT (p=2):  +4alpha  (two refractions + one internal reflection)
        switch (p) {
            case 0:
                sinThetaOp = sinThetaO * m_cos2kAlpha[1] - cosThetaO * m_sin2kAlpha[1];
                cosThetaOp = cosThetaO * m_cos2kAlpha[1] + sinThetaO * m_sin2kAlpha[1];
                break;
            case 1:
                sinThetaOp = sinThetaO * m_cos2kAlpha[0] + cosThetaO * m_sin2kAlpha[0];
                cosThetaOp = cosThetaO * m_cos2kAlpha[0] - sinThetaO * m_sin2kAlpha[0];
                break;
            case 2:
                sinThetaOp = sinThetaO * m_cos2kAlpha[2] + cosThetaO * m_sin2kAlpha[2];
                cosThetaOp = cosThetaO * m_cos2kAlpha[2] - sinThetaO * m_sin2kAlpha[2];
                break;
            default:
                sinThetaOp = sinThetaO;
                cosThetaOp = cosThetaO;
                break;
        }
        cosThetaOp = std::abs(cosThetaOp);
    }


    // Public methods

    Color3f HairBxDF::f(const Vector3f &wo, const Vector3f &wi) const {
        // Decompose directions
        float sinThetaO = wo.z();
        float cosThetaO = SafeSqrt(1.f - Sqr(sinThetaO));
        float phiO = std::atan2(wo.y(), wo.x());

        float sinThetaI = wi.z();
        float cosThetaI = SafeSqrt(1.f - Sqr(sinThetaI));
        float phiI = std::atan2(wi.y(), wi.x());

        if (std::abs(sinThetaI) < 1e-6f) return Color3f(0.f);

        float sinThetaT = sinThetaO / m_eta;
        float cosThetaT = SafeSqrt(1.f - Sqr(sinThetaT));

        // Refraction geometry
        float etap = (cosThetaO > 1e-6f)
                         ? SafeSqrt(Sqr(m_eta) - Sqr(sinThetaO)) / cosThetaO
                         : m_eta;
        float sinGammaT = std::clamp(m_h / etap, -1.f, 1.f);
        float cosGammaT = SafeSqrt(1.f - Sqr(sinGammaT));
        float gammaT = std::asin(sinGammaT);
        float cosGammaO = SafeSqrt(1.f - Sqr(m_h));

        // Per-lobe attenuation
        auto ap = computeAp(cosThetaO, cosGammaO, cosGammaT, cosThetaT);

        // Accumulate lobes
        float phi = phiI - phiO;
        Color3f fsum(0.f);

        constexpr float pruningThreshold = 1e-5f;

        for (int p = 0; p < pMax; ++p) {
            // Lobe pruning
            if (ap[p].luminance() < pruningThreshold) continue;

            float sinThetaOp, cosThetaOp;
            tiltedAngles(p, sinThetaO, cosThetaO, sinThetaOp, cosThetaOp);

            fsum += ap[p]
                    * Mp(cosThetaI, cosThetaOp, sinThetaI, sinThetaOp, m_v[p])
                    * Np(phi, p, m_s, m_gammaO, gammaT);
        }

        // Residual lobe: uniform azimuthal distribution
        if (ap[pMax].luminance() >= pruningThreshold)
            fsum += ap[pMax]
                    * Mp(cosThetaI, cosThetaO, sinThetaI, sinThetaO, m_v[pMax])
                    * (0.5f * InvPi);

        Color3f res = fsum / std::abs(sinThetaI);

        if (res.hasNaNs() || std::isinf(res.luminance())) {
            return Color3f(0.f);
        }

        return res;
    }

    float HairBxDF::pdf(const Vector3f &wo, const Vector3f &wi) const {
        // Decompose directions
        float sinThetaO = wo.z();
        float cosThetaO = SafeSqrt(1.f - Sqr(sinThetaO));
        float phiO = std::atan2(wo.y(), wo.x());

        float sinThetaI = wi.z();
        float cosThetaI = SafeSqrt(1.f - Sqr(sinThetaI));
        float phiI = std::atan2(wi.y(), wi.x());

        // Refraction geometry
        float etap = (cosThetaO > 1e-6f)
                         ? SafeSqrt(Sqr(m_eta) - Sqr(sinThetaO)) / cosThetaO
                         : m_eta;
        float sinGammaT = std::clamp(m_h / etap, -1.f, 1.f);
        float cosGammaT = SafeSqrt(1.f - Sqr(sinGammaT));
        float gammaT = std::asin(sinGammaT);
        float cosGammaO = SafeSqrt(1.f - Sqr(m_h));

        float sinThetaT = sinThetaO / m_eta;
        float cosThetaT = SafeSqrt(1.f - Sqr(sinThetaT));

        // Lobe-selection weights
        auto ap = computeAp(cosThetaO, cosGammaO, cosGammaT, cosThetaT);
        auto apPdf = computeApPdf(ap);

        float phi = phiI - phiO;
        float pdfSum = 0.f;

        const float pruningThreshold = 1e-5f;

        for (int p = 0; p < pMax; ++p) {
            if (ap[p].luminance() < pruningThreshold) continue;

            float sinThetaOp, cosThetaOp;
            tiltedAngles(p, sinThetaO, cosThetaO, sinThetaOp, cosThetaOp);

            pdfSum += apPdf[p]
                    * Mp(cosThetaI, cosThetaOp, sinThetaI, sinThetaOp, m_v[p])
                    * Np(phi, p, m_s, m_gammaO, gammaT);
        }

        // Residual lobe: uniform on the sphere
        if (ap[pMax].luminance() >= pruningThreshold)
            pdfSum += apPdf[pMax]
                    * Mp(cosThetaI, cosThetaO, sinThetaI, sinThetaO, m_v[pMax])
                    * (0.5f * InvPi);

        if (std::isnan(pdfSum) || std::isinf(pdfSum)) {
            return 0.f;
        }

        return pdfSum;
    }

    Color3f HairBxDF::sample(const Vector3f &wo, Vector3f &wi,
                             const Point2f &sample, float uc, float &pdfOut,
                             BxDFType *sampledType) const {
        // Decompose outgoing direction
        float sinThetaO = wo.z();
        float cosThetaO = SafeSqrt(1.f - Sqr(sinThetaO));
        float phiO = std::atan2(wo.y(), wo.x());

        // Refraction geometry
        float sinThetaT = sinThetaO / m_eta;
        float cosThetaT = SafeSqrt(1.f - Sqr(sinThetaT));
        float etap = (cosThetaO > 1e-6f)
                         ? SafeSqrt(Sqr(m_eta) - Sqr(sinThetaO)) / cosThetaO
                         : m_eta;
        float sinGammaT = std::clamp(m_h / etap, -1.f, 1.f);
        float cosGammaT = SafeSqrt(1.f - Sqr(sinGammaT));
        float gammaT = std::asin(sinGammaT);
        float cosGammaO = SafeSqrt(1.f - Sqr(m_h));

        // Build lobe-selection CDF from Ap weights
        auto ap = computeAp(cosThetaO, cosGammaO, cosGammaT, cosThetaT);
        auto apPdf = computeApPdf(ap);

        // Select lobe p
        int p = pMax;
        float remapUc = uc;
        for (int i = 0; i < pMax; ++i) {
            if (remapUc < apPdf[i]) {
                p = i;
                break;
            }
            remapUc -= apPdf[i];
        }
        float uFilament = (apPdf[p] > 0.f)
                          ? std::clamp(remapUc / apPdf[p], 0.f, 1.f - 1e-5f)
                          : 0.f;

        // Sample the longitudinal angle thetaI
        float sinThetaOp, cosThetaOp;
        tiltedAngles(p, sinThetaO, cosThetaO, sinThetaOp, cosThetaOp);

        float u1 = sample.x();
        float expTerm = (m_v[p] <= 0.1f) ? std::exp(-2.f / m_v[p]) : 0.f;
        float logArg = std::max(u1 + (1.f - u1) * expTerm, 1e-7f);
        float cosTheta = 1.f + m_v[p] * std::log(logArg);
        cosTheta = std::clamp(cosTheta, -1.f, 1.f);
        float sinTheta = SafeSqrt(1.f - Sqr(cosTheta));

        // uFilament drives the azimuthal rotation within the longitudinal cone
        float cosPhi_mp = std::cos(TwoPi * uFilament);
        float sinThetaI = -cosTheta * sinThetaOp + sinTheta * cosPhi_mp * cosThetaOp;
        sinThetaI = std::clamp(sinThetaI, -1.f, 1.f);
        float cosThetaI = SafeSqrt(1.f - Sqr(sinThetaI));

        // Sample the azimuthal angle theta
        float dphi;
        if (p < pMax)
            dphi = Phi(p, m_gammaO, gammaT) + SampleTrimmedLogistic(sample.y(), m_s, -Pi, Pi);
        else
            dphi = TwoPi * sample.y();

        float phiI = phiO + dphi;

        // Construct the sampled direction in the hair local frame
        wi = Vector3f(cosThetaI * std::cos(phiI),
                      cosThetaI * std::sin(phiI),
                      sinThetaI);
        wi = Normalize(wi);

        pdfOut = pdf(wo, wi);

        if (sampledType) *sampledType = type;

        if (pdfOut == 0.f) return Color3f(0.f);

        if (pdfOut < 1e-6f || std::abs(wi.z()) < 1e-6f) {
            pdfOut = 0.f;
            return Color3f(0.f);
        }

        Color3f res = f(wo, wi);
        if (res.hasNaNs()) {
            pdfOut = 0.f;
            return Color3f(0.f);
        }

        return res;
    }


    // Hair Material

    class HairMaterial : public Material {
    public:
        explicit HairMaterial(const PropertyList &props) : Material(props) {
            m_beta_m = props.getFloat("beta_m", 0.3f);
            m_beta_n = props.getFloat("beta_n", 0.3f);
            m_alpha = props.getFloat("alpha", 2.0f);
            m_eta = props.getFloat("eta", 1.55f);

            // Gradient curve parameter
            m_tip_bias = props.getFloat("tip_bias", 2.0f);

            if (props.hasColor("sigma_a")) {
                // Direct absorption coefficient
                m_sigma_a_root = props.getColor("sigma_a");
                m_sigma_a_tip = m_sigma_a_root;
            } else if (props.hasColor("color")) {
                // Perceptual hair colour (inverted via d'Eon fit)
                m_sigma_a_root = SigmaAFromReflectance(props.getColor("color"), m_beta_n);
                m_sigma_a_tip = m_sigma_a_root;
            } else if (props.hasColor("color_root")) {
                Color3f rootColor = props.getColor("color_root", Color3f(0.1f));
                Color3f tipColor  = props.getColor("color_tip", rootColor);
                m_sigma_a_root = SigmaAFromReflectance(rootColor, m_beta_n);
                m_sigma_a_tip  = SigmaAFromReflectance(tipColor, m_beta_n);
            } else {
                // Melanin concentrations (default: dark brown)
                float eu = props.getFloat("eumelanin", 1.3f);
                float ph = props.getFloat("pheomelanin", 0.2f);
                m_sigma_a_root = SigmaAFromConcentration(eu, ph);
                m_sigma_a_tip = m_sigma_a_root;
            }
        }

        void computeScatteringFunctions(SurfaceInteraction &isect,
                                        MemoryArena &arena) const override {
            applyNormalOrBump(isect);
            applyMediums(isect);

            float h = (isect.uv.y() > 0.f || isect.uv.y() < 0.f)
                          ? std::clamp(-1.f + 2.f * isect.uv.y(), -1.f, 1.f)
                          : 0.f;

            float u = std::clamp(isect.uv.x(), 0.0f, 1.0f);
            float tipWeight = std::pow(u, m_tip_bias);
            Color3f current_sigma_a = m_sigma_a_root * (1.0f - tipWeight) + m_sigma_a_tip * tipWeight;

            Normal3f shadingNormal = isect.n;
            Vector3f shadingTangent = isect.dpdu;
            isect.n = Normal3f(shadingTangent);
            isect.dpdu = Vector3f(shadingNormal);

            isect.bsdf = arena.create<BSDF>(isect, m_eta);
            isect.bsdf->add(arena.create<HairBxDF>(
                h, m_eta, current_sigma_a, m_beta_m, m_beta_n, m_alpha));
        }

        Color3f getAlbedo(const SurfaceInteraction &) const override {
            return ColorExp(-m_sigma_a_root);
        }

        std::string toString() const override {
            return std::format(
                "HairMaterial[\n"
                "  eta     = {}\n"
                "  sigma_a = {}\n"
                "  beta_m  = {}\n"
                "  beta_n  = {}\n"
                "  alpha   = {} degrees\n"
                "]",
                m_eta,
                m_sigma_a_root.toString(),
                m_beta_m, m_beta_n, m_alpha);
        }

    private:
        float m_eta;
        Color3f m_sigma_a_root;
        Color3f m_sigma_a_tip;
        float m_tip_bias;
        float m_beta_m;
        float m_beta_n;
        float m_alpha;
    };

    GND_REGISTER_CLASS(HairMaterial, "hair");
}