#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/textures/texture.h>
#include <cmath>

#include "gianduia/math/perlin.h"

namespace gnd {

    template <ProceduralTextureValue T>
    class MarbleTexture : public Texture<T> {
    public:
        explicit MarbleTexture(const PropertyList& props) {
            if constexpr (std::same_as<T, float>) {
                m_value1 = props.getFloat("value1", 0.0f);
                m_value2 = props.getFloat("value2", 1.0f);
            } else {
                m_value1 = props.getColor("value1", Color3f(0.1f));
                m_value2 = props.getColor("value2", Color3f(0.9f));
            }

            m_scale = props.getFloat("scale", 1.0f);
            m_octaves = props.getInteger("octaves", 6);

            m_veinFreq = props.getFloat("vein_freq", 10.0f);
            m_turbScale = props.getFloat("turb_scale", 5.0f);

            m_axis = Normalize(props.getVector3("axis", Vector3f(0.0f, 1.0f, 0.0f)));
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            float turb = Perlin::turbulence(isect.p / m_scale, m_octaves);
            float val = Dot(Vector3f(isect.p), m_axis) + turb * m_turbScale;
            float sineval = std::sin(val * m_veinFreq);

            float t = 0.5f * (1.0f + sineval);

            if constexpr (std::same_as<T, float>) {
                return std::lerp(m_value1, m_value2, t);
            } else {
                return m_value1 * (1.0f - t) + m_value2 * t;
            }
        }

        std::string toString() const override {
            return std::format(
                "MarbleTexture([\n"
                "  axis = {}\n"
                "  scale = {}\n"
                "  octaves = {}\n"
                "  veinFreq = {}\n"
                "  turbScale = {}\n"
                "]",
                m_axis.toString(), m_scale, m_octaves, m_veinFreq, m_turbScale
            );
        }

    private:
        T m_value1;
        T m_value2;
        float m_scale;
        int m_octaves;
        
        float m_veinFreq;
        float m_turbScale;
        Vector3f m_axis;
    };

}