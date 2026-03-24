#pragma once

#include <gianduia/math/geometry.h>
#include <gianduia/math/constants.h>
#include <gianduia/math/warp.h>

namespace gnd {

    class PhaseFunction {
    public:
        virtual ~PhaseFunction() = default;

        /// Evaluates phase function
        /// @param wo Outgoing direction (known ray; towards camera)
        /// @param wi Incident direction (next ray; towards light)
        /// @return Value of the phase function
        virtual float p(const Vector3f& wo, const Vector3f& wi) const = 0;

        /// Samples a scattering direction wi and returns the value of the phase function
        /// @param wo Outgoing direction (known)
        /// @param wi [out] Sampled incident direction
        /// @param u Uniform 2D sample
        /// @return Evaluated value p(wo, wi)
        virtual float sample(const Vector3f& wo, Vector3f* wi, const Point2f& u) const = 0;
    };


    class IsotropicPhaseFunction : public PhaseFunction {
    public:
        float p(const Vector3f& wo, const Vector3f& wi) const override {
            return Inv4Pi; 
        }

        float sample(const Vector3f& wo, Vector3f* wi, const Point2f& u) const override {
            *wi = Warp::squareToUniformSphere(u);
            return Inv4Pi;
        }
    };

    class HenyeyGreensteinPhaseFunction : public PhaseFunction {
    public:
        // g deve essere nel range (-1, 1). Lo clampiamo leggermente per evitare instabilità numeriche
        explicit HenyeyGreensteinPhaseFunction(float g) : m_g(std::clamp(g, -0.999f, 0.999f)) {}

        float p(const Vector3f& wo, const Vector3f& wi) const override {
            return phaseHG(Dot(wo, wi), m_g);
        }

        float sample(const Vector3f& wo, Vector3f* wi, const Point2f& u) const override {
            float cosTheta;

            // Gestione del caso isotropico (o quasi isotropico) per evitare divisioni per zero
            if (std::abs(m_g) < 1e-3f) {
                cosTheta = 1.0f - 2.0f * u.x();
            } else {
                float sqrTerm = (1.0f - m_g * m_g) / (1.0f - m_g + 2.0f * m_g * u.x());
                cosTheta = (1.0f + m_g * m_g - sqrTerm * sqrTerm) / (2.0f * m_g);
            }

            float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
            float phi = 2.0f * M_PI * u.y();

            // Calcolo del sistema di coordinate locale attorno a wo
            Vector3f v1, v2;
            coordinateSystem(wo, &v1, &v2);

            // Costruzione del nuovo vettore direzione campionato (wi)
            *wi = v1 * (sinTheta * std::cos(phi)) +
                  v2 * (sinTheta * std::sin(phi)) +
                  wo * cosTheta;

            return phaseHG(cosTheta, m_g);
        }

    private:
        float m_g;

        static float phaseHG(float cosTheta, float g) {
            float denom = 1.0f + g * g + 2.0f * g * cosTheta;
            return Inv4Pi * (1.0f - g * g) / (denom * std::sqrt(denom));
        }

        static void coordinateSystem(const Vector3f& v1, Vector3f* v2, Vector3f* v3) {
            if (std::abs(v1.x()) > std::abs(v1.y())) {
                float invLen = 1.0f / std::sqrt(v1.x() * v1.x() + v1.z() * v1.z());
                *v2 = Vector3f(-v1.z() * invLen, 0.0f, v1.x() * invLen);
            } else {
                float invLen = 1.0f / std::sqrt(v1.y() * v1.y() + v1.z() * v1.z());
                *v2 = Vector3f(0.0f, v1.z() * invLen, -v1.y() * invLen);
            }
            *v3 = Cross(v1, *v2);
        }
    };

}