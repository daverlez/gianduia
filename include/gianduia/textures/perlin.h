#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/textures/texture.h>
#include <cmath>

#include "gianduia/math/perlin.h"

namespace gnd {

    template <ProceduralTextureValue T>
    class PerlinNoiseTexture : public Texture<T> {
    public:
        explicit PerlinNoiseTexture(const PropertyList& props) {
            if constexpr (std::same_as<T, float>) {
                m_value1 = props.getFloat("value1", 0.0f);
                m_value2 = props.getFloat("value2", 1.0f);
            } else {
                m_value1 = props.getColor("value1", Color3f(0.0f));
                m_value2 = props.getColor("value2", Color3f(1.0f));
            }

            m_scale = props.getFloat("scale", 1.0f);
            m_octaves = props.getInteger("octaves", 6); 
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            float noiseVal = Perlin::turbulence(isect.p / m_scale, m_octaves);

            if constexpr (std::same_as<T, float>) {
                return std::lerp(m_value1, m_value2, noiseVal);
            } else {
                return m_value1 * (1.0f - noiseVal) + m_value2 * noiseVal;
            }
        }

        std::string toString() const override {
            return std::format(
                "PerlinNoiseTexture([\n"
                "  scale = {}\n"
                "  octaves = {}\n"
                "]",
                m_scale, m_octaves
            );
        }

    private:
        T m_value1;
        T m_value2;
        float m_scale;
        int m_octaves;
    };

}
