#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/textures/texture.h>

namespace gnd {

    template <ValidTextureValue T>
    class ConstantTexture : public Texture<T> {
    public:
        explicit ConstantTexture(const PropertyList& props) {
            if constexpr (std::same_as<T, float>)
                m_value = props.getFloat("value", 1.0f);
            else
                m_value = props.getColor("value", Color3f(1.0f));
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            return m_value;
        }

        std::string toString() const override {
            if constexpr (std::same_as<T, float>)
                return std::format("ConstantTexture(float)[value={:.2f}]", m_value);
            else
                return std::format("ConstantTexture(color)[value={}]", m_value.toString());
        }

    private:
        T m_value;
    };


    template <>
    inline std::string ConstantTexture<float>::toString() const {
        return std::format("ConstantTexture(float)[value={:.2f}]", m_value);
    }

    template <>
    inline std::string ConstantTexture<Color3f>::toString() const {
        return std::format("ConstantTexture(color)[value={}]", m_value.toString());
    }

}