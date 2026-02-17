#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/textures/texture.h>

namespace gnd {

    template <typename T>
    class ConstantTexture : public Texture<T> {
    public:
        explicit ConstantTexture(const PropertyList& props);

        T evaluate(const SurfaceInteraction& isect) const override {
            return m_value;
        }

        std::string toString() const override {
            return "ConstantTexture[]";
        }

    private:
        T m_value;
    };

    template <>
    inline ConstantTexture<float>::ConstantTexture(const PropertyList& props) {
        m_value = props.getFloat("value", 1.0f);
    }

    template <>
    inline ConstantTexture<Color3f>::ConstantTexture(const PropertyList& props) {
        m_value = props.getColor("value", Color3f(1.0f));
    }

    template <>
    inline std::string ConstantTexture<float>::toString() const {
        return std::format("ConstantTexture(float)[value={:.2f}]", m_value);
    }

    template <>
    inline std::string ConstantTexture<Color3f>::toString() const {
        return std::format("ConstantTexture(color)[value={}]", m_value.toString());
    }

}