#pragma once
#include <gianduia/math/color.h>
#include <cmath>
#include <algorithm>

namespace gnd {



    /// Computes the Fresnel term for dielectrics
    /// @param cosThetaI cosine of incidence angle. Can be negative.
    /// @param etaExt external index of refraction. Automatically swaps it if cosThetaI < 0.
    /// @param etaInt internal index of refraction. Automatically swaps it if cosThetaI < 0
    inline float FrDielectric(float cosThetaI, float etaExt, float etaInt) {
        cosThetaI = std::clamp(cosThetaI, -1.f, 1.f);

        // If getting out of the dielectric, swap the two indices of refraction
        if (cosThetaI < 0.f) {
            std::swap(etaExt, etaInt);
            cosThetaI = -cosThetaI;
        }

        // Snell's law
        float sinThetaI = std::sqrt(std::max(0.f, 1.f - cosThetaI * cosThetaI));
        float sinThetaT = etaExt / etaInt * sinThetaI;

        // Handling total internal reflection
        if (sinThetaT >= 1.f) return 1.f;

        float cosThetaT = std::sqrt(std::max(0.f, 1.f - sinThetaT * sinThetaT));

        // Applying Fresnel formulas
        float rParl = ((etaInt * cosThetaI) - (etaExt * cosThetaT)) /
                      ((etaInt * cosThetaI) + (etaExt * cosThetaT));
        float rPerp = ((etaExt * cosThetaI) - (etaInt * cosThetaT)) /
                      ((etaExt * cosThetaI) + (etaInt * cosThetaT));

        return (rParl * rParl + rPerp * rPerp) / 2.f;
    }

    /// Computes the refracted vector. Returns false in case of total internal reflection.
    /// @param wi Incident direction (pointing away from the interaction point)
    /// @param n Surface normal (facing the incident side)
    /// @param eta The eta ratio (eta incident / eta transmitted)
    /// @param wt [out] the transmitted direction (pointing away from the interaction point)
    inline bool Refract(const Vector3f& wi, const Normal3f& n, float eta, Vector3f* wt) {
        float cosThetaI = Dot(n, wi);
        float sin2ThetaI = std::max(0.f, 1.f - cosThetaI * cosThetaI);
        float sin2ThetaT = eta * eta * sin2ThetaI;

        if (sin2ThetaT >= 1.f) return false;

        float cosThetaT = std::sqrt(1.f - sin2ThetaT);

        *wt = -wi * eta + n * (eta * cosThetaI - cosThetaT);
        return true;
    }


    class Fresnel {
    public:
        virtual ~Fresnel() = default;

        virtual Color3f evaluate(float cosThetaI) const = 0;
    };


    class FresnelDielectric : public Fresnel {
    public:
        FresnelDielectric(float etaI, float etaT) : m_etaI(etaI), m_etaT(etaT) {}

        Color3f evaluate(float cosThetaI) const override {
            return Color3f(FrDielectric(cosThetaI, m_etaI, m_etaT));
        }

    private:
        float m_etaI, m_etaT;
    };


    class FresnelNoOp : public Fresnel {
    public:
        Color3f evaluate(float cosThetaI) const override {
            return Color3f(1.f);
        }
    };

    class FresnelConductor : public Fresnel {
    public:
        FresnelConductor(const Color3f& etaI, const Color3f& etaT, const Color3f& k)
            : m_etaI(etaI), m_etaT(etaT), m_k(k) {}

        Color3f evaluate(float cosThetaI) const override {
            float cosThetaI2 = cosThetaI * cosThetaI;
            float sinThetaI2 = std::max(0.0f, 1.f - cosThetaI2);

            auto calculateChannel = [&](float eta_i, float eta_t, float k_c) {
                float eta = eta_t / eta_i;
                float etak = k_c / eta_i;
                float eta2 = eta * eta;
                float etak2 = etak * etak;

                float t0 = eta2 - etak2 - sinThetaI2;
                float a2plusb2 = std::sqrt(t0 * t0 + 4.f * eta2 * etak2);
                float t1 = a2plusb2 + cosThetaI2;
                float a = std::sqrt(std::max(0.0f, 0.5f * (a2plusb2 + t0)));
                float t2 = 2.f * std::abs(cosThetaI) * a;
                float Rs = (t1 - t2) / (t1 + t2);

                float t3 = cosThetaI2 * a2plusb2 + sinThetaI2 * sinThetaI2;
                float t4 = t2 * sinThetaI2;
                float Rp = Rs * (t3 - t4) / (t3 + t4);

                return 0.5f * (Rp + Rs);
            };

            return Color3f(
                calculateChannel(m_etaI.r(), m_etaT.r(), m_k.r()),
                calculateChannel(m_etaI.g(), m_etaT.g(), m_k.g()),
                calculateChannel(m_etaI.b(), m_etaT.b(), m_k.b())
            );
        }

    private:
        Color3f m_etaI, m_etaT, m_k;
    };

}