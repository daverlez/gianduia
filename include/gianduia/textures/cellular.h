#pragma once

#include <gianduia/textures/texture.h>
#include <gianduia/math/worley.h>
#include <algorithm>

namespace gnd {
    template<ProceduralTextureValue T>
    class CellularTexture : public Texture<T> {
    public:
        explicit CellularTexture(const PropertyList& props) {
            if constexpr (std::same_as<T, float>) {
                m_value1 = props.getFloat("value1", 0.0f);
                m_value2 = props.getFloat("value2", 1.0f);
            } else {
                m_value1 = props.getColor("value1", Color3f(0.0f));
                m_value2 = props.getColor("value2", Color3f(1.0f));
            }

            m_scale = props.getFloat("scale", 1.0f);
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            WorleyResult wr = Worley::noise(isect.p / m_scale);

            float noiseVal = wr.f1;
            noiseVal = std::clamp(noiseVal, 0.0f, 1.0f);

            return Lerp(noiseVal, m_value1, m_value2);
        }

        std::string toString() const override {
            return std::format(
                "CellularTexture([\n"
                "  scale = {}\n"
                "]",
                m_scale
            );
        }

    private:
        T m_value1;
        T m_value2;
        float m_scale;
    };
}