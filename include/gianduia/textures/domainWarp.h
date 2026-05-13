#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/textures/texture.h>
#include <format>
#include <stdexcept>

namespace gnd {

    template <ValidTextureValue T>
    class DomainWarpTexture : public Texture<T> {
    public:
        explicit DomainWarpTexture(const PropertyList& props) {
            m_strength = props.getFloat("strength", 1.0f);
        }

        virtual void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() == ETexture) {
                if (child->getName() == "base") {
                    if (m_baseTexture) throw std::runtime_error("WarpTexture: base texture already defined!");
                    m_baseTexture = std::static_pointer_cast<Texture<T>>(child);
                } 
                else if (child->getName() == "warp") {
                    if (m_warpTexture) throw std::runtime_error("WarpTexture: warp texture already defined!");
                    m_warpTexture = std::static_pointer_cast<Texture<Color3f>>(child);
                }
            } else {
                throw std::runtime_error("WarpTexture: cannot add specified child!");
            }
        }

        virtual void activate() override {
            if (!m_baseTexture) throw std::runtime_error("WarpTexture: missing base texture!");
            if (!m_warpTexture) throw std::runtime_error("WarpTexture: missing warp texture!");
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            Color3f warpVal = m_warpTexture->evaluate(isect);
            Vector3f warpDir(
                (warpVal.r() * 2.0f - 1.0f) * m_strength,
                (warpVal.g() * 2.0f - 1.0f) * m_strength,
                (warpVal.b() * 2.0f - 1.0f) * m_strength
            );

            SurfaceInteraction warpedIsect = isect;
            warpedIsect.p = isect.p + warpDir;

            Vector3f dpdv = Cross(isect.n, isect.dpdu);
            float lengthSqU = isect.dpdu.lengthSquared();
            float lengthSqV = dpdv.lengthSquared();
            float du = (lengthSqU > 0.0f) ? Dot(warpDir, isect.dpdu) / lengthSqU : 0.0f;
            float dv = (lengthSqV > 0.0f) ? Dot(warpDir, dpdv) / lengthSqV : 0.0f;

            warpedIsect.uv.x() += du;
            warpedIsect.uv.y() += dv;

            return m_baseTexture->evaluate(warpedIsect);
        }

        std::string toString() const override {
            return std::format(
                "DomainWarpTexture([\n"
                "  strength = {},\n"
                "  base = {},\n"
                "  warp = {}\n"
                "]",
                m_strength,
                indent(m_baseTexture->toString(), 2),
                indent(m_warpTexture->toString(), 2)
            );
        }

    private:
        std::shared_ptr<Texture<T>> m_baseTexture;
        std::shared_ptr<Texture<Color3f>> m_warpTexture;
        float m_strength;
    };

}