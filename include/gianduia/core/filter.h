#pragma once
#include <gianduia/math/geometry.h>
#include <gianduia/core/object.h>

namespace gnd {

    class Filter : public GndObject{
    public:
        Filter(float radiusX, float radiusY) : m_radius(radiusX, radiusY) {}
        virtual ~Filter() = default;

        virtual float evaluate(const Point2f& p) const = 0;
        Vector2f getRadius() const { return m_radius; }

        EClassType getClassType() const override { return EFilter; }
    protected:
        Vector2f m_radius;
    };

    class BoxFilter : public Filter {
    public:
        BoxFilter(const PropertyList& props) : Filter(0.5f, 0.5f) {}

        float evaluate(const Point2f& p) const override {
            if (std::abs(p.x()) <= m_radius.x() && std::abs(p.y()) <= m_radius.y()) return 1.0f;
            return 0.0f;
        }

        std::string toString() const override {
            return "BoxFilter[]";
        }
    };

    class GaussianFilter : public Filter {
    public:
        GaussianFilter(const PropertyList& props) : Filter(2.0f, 2.0f) {
            m_radius = props.getVector2("radius", Vector2f(2.0f, 2.0f));
            m_alpha = props.getFloat("alpha", 2.0f);
            m_expX = std::exp(-m_alpha * m_radius.x() * m_radius.x());
            m_expY = std::exp(-m_alpha * m_radius.y() * m_radius.y());
        }

        float evaluate(const Point2f& p) const override {
            float gx = std::max(0.0f, std::exp(-m_alpha * p.x() * p.x()) - m_expX);
            float gy = std::max(0.0f, std::exp(-m_alpha * p.y() * p.y()) - m_expY);
            return gx * gy;
        }

        std::string toString() const override {
            return "GaussianFilter[]";
        }
    private:
        float m_alpha, m_expX, m_expY;
    };
}
