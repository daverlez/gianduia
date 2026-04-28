#pragma once
#include <gianduia/materials/bxdf.h>
#include <array>

namespace gnd {
    /// Maximum number of scattering path segments considered (R, TT, TRT, residual).
    static constexpr int pMax = 3;

    // Sigma_a conversion utilities

    // Donner et al. (2006)
    /// Converts eumelanin and pheomelanin concentrations to absorption coefficient sigma_a.
    Color3f SigmaAFromConcentration(float eumelanin, float pheomelanin);

    /// d'Eon et al. (2011)
    /// Converts perceived hair colour to absorption coefficient sigma_a.
    Color3f SigmaAFromReflectance(const Color3f &reflectance, float beta_n);


    // Implementation of the Marschner et al. (2003) scattering model with
    // the energy-conserving extensions of d'Eon et al. (2011).
    class HairBxDF : public BxDF {
    public:
        /// @param h        Lateral offset along the hair cross-section in [−1, 1].
        /// @param eta      Index of refraction of the hair cortex.
        /// @param sigma_a  Absorption coefficient of the cortex.
        /// @param beta_m   Longitudinal roughness in [0, 1].
        /// @param beta_n   Azimuthal roughness in [0, 1].
        /// @param alpha    Cuticle scale tilt angle in degrees.
        HairBxDF(float h, float eta, const Color3f &sigma_a,
                 float beta_m, float beta_n, float alpha = 2.0f);

        /// Evaluates the hair scattering function, with directions expressed in the hair local frame
        Color3f f(const Vector3f &wo, const Vector3f &wi) const override;

        Color3f sample(const Vector3f &wo, Vector3f &wi,
                       const Point2f &sample, float uc, float &pdf,
                       BxDFType *sampledType = nullptr) const override;

        float pdf(const Vector3f &wo, const Vector3f &wi) const override;

    private:
        /// Computes the per-lobe attenuation array A_p[0..pMax] (Fresnel × absorption).
        std::array<Color3f, pMax + 1> computeAp(float cosThetaO, float cosGammaO, float cosGammaT,
                                                float cosThetaT) const;

        /// Builds the normalised discrete lobe-selection weights from an Ap array.
        std::array<float, pMax + 1> computeApPdf(const std::array<Color3f, pMax + 1> &ap) const;

        /// Computes the shifted (tilt-corrected) sinThetaO and cosThetaO for lobe p.
        void tiltedAngles(int p, float sinThetaO, float cosThetaO, float &sinThetaOp, float &cosThetaOp) const;

        // Geometry
        float m_h;
        float m_gammaO;
        float m_eta;

        // Appearance
        Color3f m_sigma_a;
        float m_beta_m;
        float m_beta_n;
        float m_alpha;

        // Precomputed
        float m_v[pMax + 1];    // Longitudinal variance per lobe
        float m_s;              // Azimuthal logistic scale factor
        float m_sin2kAlpha[3];
        float m_cos2kAlpha[3];
    };
} // namespace gnd
