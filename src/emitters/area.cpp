#include "gianduia/core/emitter.h"
#include "gianduia/core/factory.h"
#include "gianduia/shapes/primitive.h"
#include "gianduia/textures/texture.h"

namespace gnd {

    class AreaLight : public Emitter {
    public:
        AreaLight(const PropertyList& props) : Emitter(EMITTER_AREA) {
            if (props.hasColor("radiance")) {
                PropertyList p;
                p.setColor("value", props.getColor("radiance"));
                m_radiance = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
        }

        void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() != ETexture)
                throw std::runtime_error("AreaLight: cannot add specified child!");

            if (child->getName() == "radiance") {
                if (m_radiance)
                    throw std::runtime_error("AreaLight: there's already a radiance value defined!");
                m_radiance = std::static_pointer_cast<Texture<Color3f>>(child);
            }
        }

        void activate() override {
            if (!m_radiance) {
                // Default: missing texture checkerboard pattern
                PropertyList p;
                p.setColor("value1", Color3f(0.0f));
                p.setColor("value2", Color3f(1.0f, 0.0f, 1.0f));
                p.setVector2("scale", Vector2f(0.1f, 0.1f));
                m_radiance = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("checkerboard_color", p)));
                m_radiance->activate();
            }
        }

        Color3f eval(const SurfaceInteraction &isect, const Vector3f &w) const override {
            if (Dot(isect.n, w) > 0.0f)
                return m_radiance->evaluate(isect);
            return Color3f(0.0f);
        }

        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const override {
            SurfaceInteraction local;
            const Transform& toWorld = m_primitive->getToWorld(ref.time);
            float areaPdf = m_primitive->getShape()->sampleSurface(ref.p, sample, local);

            info.p = toWorld(local.p);
            info.n = Normalize(toWorld(local.n));
            info.uv = local.uv;
            info.time = ref.time;

            Frame frame(Vector3f(local.n.x(), local.n.y(), local.n.z()));
            Vector3f t1World = toWorld(frame.x);
            Vector3f t2World = toWorld(frame.y);

            float areaScale = Cross(t1World, t2World).length();
            if (areaScale > 0.0f) {
                areaPdf /= areaScale;
            }

            shadowRay.o = ref.p;
            shadowRay.d = Normalize(info.p - ref.p);
            shadowRay.tMin = Epsilon;
            shadowRay.tMax = (info.p - ref.p).length() - Epsilon;
            shadowRay.time = ref.time;

            float dist = (ref.p - info.p).lengthSquared();
            Vector3f wi = shadowRay.d;
            float cosTheta = Dot(info.n, -wi);

            if (areaPdf <= Epsilon || cosTheta <= Epsilon)
                return Color3f(0.0f);

            float anglePdf = areaPdf * dist / cosTheta;
            pdf = anglePdf;

            return eval(info, Normalize(ref.p - info.p)) / pdf;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const {
            SurfaceInteraction local;
            const Transform toWorld = m_primitive->getToWorld(ref.time);
            const Transform toLocal = toWorld.inverse();
            local.p = toLocal(info.p);
            local.n = toLocal(info.n);
            local.uv = info.uv;

            float areaPdf = m_primitive->getShape()->pdfSurface(toLocal(ref.p), local);

            Frame frame(Vector3f(local.n.x(), local.n.y(), local.n.z()));
            Vector3f t1World = toWorld(frame.x);
            Vector3f t2World = toWorld(frame.y);

            float areaScale = Cross(t1World, t2World).length();
            if (areaScale > 0.0f) {
                areaPdf /= areaScale;
            }

            // Converting pdf from area measure to solid angle measure
            float dist = (ref.p - info.p).lengthSquared();
            Vector3f wi = Normalize(info.p - ref.p);
            float cosTheta = Dot(info.n, -wi);

            if (areaPdf <= Epsilon || cosTheta <= Epsilon)
                return 0.0f;

            float anglePdf = areaPdf * dist / cosTheta;
            return anglePdf;
        }

        virtual Color3f samplePhoton(const Point2f& uPos, const Point2f& uDir, float time, Ray& photonRay) const override {
            SurfaceInteraction local;
            const Transform& toWorld = m_primitive->getToWorld(time);
            float areaPdf = m_primitive->getShape()->sampleSurface(Point3f(0.0f), uPos, local);

            SurfaceInteraction info;
            info.p = toWorld(local.p);
            info.n = Normalize(toWorld(local.n));
            info.uv = local.uv;
            info.time = time;

            Frame frame(Vector3f(info.n.x(), info.n.y(), info.n.z()));
            Vector3f t1World = toWorld(frame.x);
            Vector3f t2World = toWorld(frame.y);

            float areaScale = Cross(t1World, t2World).length();
            if (areaScale > 0.0f) {
                areaPdf /= areaScale;
            }

            if (areaPdf <= Epsilon) return Color3f(0.0f);

            Vector3f localDir = Warp::squareToCosineHemisphere(uDir);
            Vector3f wi = frame.toWorld(localDir);

            photonRay.o = info.p;
            photonRay.d = wi;
            photonRay.tMin = Epsilon;
            photonRay.tMax = std::numeric_limits<float>::max();
            photonRay.time = time;

            Color3f Le = m_radiance->evaluate(info);

            return (Le * Pi) / areaPdf;
        }

        std::string toString() const override {
            return std::format(
                "AreaLight[\n"
                        "  radiance =\n{}\n"
                        "]",
                        indent(m_radiance->toString(), 2));
        }

    private:
        std::shared_ptr<Texture<Color3f>> m_radiance;
    };

    GND_REGISTER_CLASS(AreaLight, "area");
}
