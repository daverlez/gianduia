#include <gianduia/materials//medium.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/sampler.h>
#include <gianduia/math/color.h>
#include <gianduia/math/warp.h>

#include <cmath>
#include <algorithm>

namespace gnd {

    class HomogeneousMedium : public Medium {
    public:
        HomogeneousMedium(const PropertyList& props) {
            m_sigma_a = props.getColor("sigma_a", Color3f(0.1f));
            m_sigma_s = props.getColor("sigma_s", Color3f(0.1f));
            m_g = props.getFloat("g", 0.0f);

            m_sigma_t = m_sigma_a + m_sigma_s;
        }

        Color3f Tr(const Ray& ray, Sampler& sampler) const override {
            if (std::isinf(ray.tMax)) return Color3f(0.0f);

            return Color3f(
                std::exp(-m_sigma_t.r() * ray.tMax),
                std::exp(-m_sigma_t.g() * ray.tMax),
                std::exp(-m_sigma_t.b() * ray.tMax)
            );
        }

        Color3f sample(const Ray& ray, Sampler& sampler, MemoryArena& arena, MediumInteraction& mi) const override {
            int channel = std::min(static_cast<int>(sampler.next1D() * 3.0f), 2);

            float dist = -std::log(1.0f - sampler.next1D()) / m_sigma_t[channel];

            float t = std::min(dist, ray.tMax);
            bool sampledMedium = t < ray.tMax;

            Color3f Tr_val(
                std::exp(-m_sigma_t.r() * t),
                std::exp(-m_sigma_t.g() * t),
                std::exp(-m_sigma_t.b() * t)
            );

            Color3f pdf_channels = sampledMedium ? (m_sigma_t * Tr_val) : Tr_val;
            float pdf = (pdf_channels.r() + pdf_channels.g() + pdf_channels.b()) / 3.0f;

            if (pdf == 0.0f) {
                return Color3f(0.0f);
            }

            if (sampledMedium) {
                mi.p = ray.o + ray.d * t;
                mi.wo = -ray.d;
                mi.medium = this;
                mi.phase = arena.create<HenyeyGreensteinPhaseFunction>(m_g);
                mi.sigma_a = m_sigma_a;
                mi.sigma_s = m_sigma_s;
            }

            return Tr_val / pdf;
        }

        virtual float getDensity(const Point3f& p) const override {
            return std::max({m_sigma_t.r(), m_sigma_t.g(), m_sigma_t.b()});
        }

        virtual std::string toString() const override {
            return std::format(
                "HomogeneousMedium[\n"
                        "  absorption = {}\n"
                        "  scattering = {}\n"
                        "  transmittance = {}\n"
                        "  anisotropy = {}\n"
                        "]",
                        m_sigma_a.toString(),
                        m_sigma_s.toString(),
                        m_sigma_t.toString(),
                        m_g);
        }

    private:
        Color3f m_sigma_a;
        Color3f m_sigma_s;
        Color3f m_sigma_t;
        float m_g;
    };

    GND_REGISTER_CLASS(HomogeneousMedium, "homogeneous");
}