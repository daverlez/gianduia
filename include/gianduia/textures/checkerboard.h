#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/textures/texture.h>

namespace gnd {

    template <ValidTextureValue T>
    class CheckerboardTexture : public Texture<T> {
    public:
        explicit CheckerboardTexture(const PropertyList& props) {
            if constexpr (std::same_as<T, float>) {
                m_value1 = props.getFloat("value1", 1.0f);
                m_value2 = props.getFloat("value2", 0.0f);
            } else {
                m_value1 = props.getColor("value1", Color3f(1.0f));
                m_value2 = props.getColor("value2", Color3f(0.0f));
            }

            m_scale = props.getVector2("scale", Vector2f(1.0f));
            m_delta = props.getPoint2("delta", Point2f(0.0f));
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            Point2f uv = isect.uv;
            uv.x() /= m_scale.x(); uv.y() /= m_scale.y();
            uv.x() -= m_delta.x(); uv.y() -= m_delta.y();

            int s = static_cast<int>(std::floor(uv.x()));
            int t = static_cast<int>(std::floor(uv.y()));

            if ((s + t) % 2 == 0)
                return m_value1;

            return m_value2;
        }

        std::string toString() const override {
            if constexpr (std::same_as<T, float>)
                return std::format(
                    "CheckerboardTexture(float)[\n"
                    "  value1 = {}\n"
                    "  value2 = {}\n"
                    "  scale = {}\n"
                    "  delta = {}\n"
                    "]",
                    m_value1, m_value2, m_scale.toString(), m_delta.toString()
                );
            else
                return std::format(
                    "CheckerboardTexture(color)[\n"
                    "  value1 = {}\n"
                    "  value2 = {}\n"
                    "  scale = {}\n"
                    "  delta = {}\n"
                    "]",
                    m_value1.toString(), m_value2.toString(), m_scale.toString(), m_delta.toString()
                );
        }

    private:
        T m_value1;
        T m_value2;
        Vector2f m_scale;
        Point2f m_delta;
    };

}