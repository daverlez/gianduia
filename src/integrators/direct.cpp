#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"

namespace gnd {
    class DirectLight : public SamplerIntegrator {
    public:
        DirectLight(const PropertyList& props) : SamplerIntegrator(props) { }

        Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena,
            Color3f* outAlbedo, Normal3f* outNormal, float* outDepth = nullptr) const override {
            SurfaceInteraction isect;

            if (!scene.rayIntersect(ray, isect))
                return Color3f(0.0f);

            Color3f L(0.0f);

            if (isect.primitive->getEmitter()) {
                L += isect.primitive->getEmitter()->eval(isect, -ray.d);
            }

            isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);
            if (!isect.bsdf) return L;

            // Next Event Estimation
            float lightSelectPdf = 1.0f / scene.getEmitters().size();
            std::shared_ptr<Emitter> emitter = scene.getRandomEmitter(sampler.next1D());

            SurfaceInteraction lightIsect;
            float lightPdf;
            Ray shadowRay;

            Color3f Li_nee = emitter->sample(isect, sampler.next2D(), lightIsect, lightPdf, shadowRay);
            Li_nee /= lightSelectPdf;

            if (lightPdf > 1e-6f && !Li_nee.isBlack() && !scene.rayIntersect(shadowRay)) {
                Vector3f wi = Normalize(lightIsect.p - isect.p);
                float bsdfPdf = isect.bsdf->pdf(-ray.d, wi);
                Color3f f = isect.bsdf->f(-ray.d, wi);

                if (!f.isBlack() && bsdfPdf > 1e-6f) {
                    float p_light = lightPdf * lightSelectPdf;
                    float p_bsdf = emitter->isDelta() ? 0.0f : bsdfPdf;

                    float misDenom = p_light + p_bsdf;
                    if (misDenom > 1e-6f) {
                        L += f * Li_nee * std::abs(Dot(isect.n, wi)) * p_light / misDenom;
                    }
                }
            }

            // BSDF Sampling
            Vector3f wi;
            float bsdfPdf;
            BxDFType sampledType;
            Color3f f = isect.bsdf->sample(-ray.d, &wi, sampler.next2D(), sampler.next1D(), &bsdfPdf, &sampledType);

            if (!f.isBlack() && bsdfPdf > 1e-6f) {
                Ray secRay(isect.p, wi);
                SurfaceInteraction secIsect;

                if (scene.rayIntersect(secRay, secIsect) && secIsect.primitive->getEmitter()) {
                    std::shared_ptr<Emitter> hitEmitter = secIsect.primitive->getEmitter();
                    Color3f Li_bsdf = hitEmitter->eval(secIsect, -wi);

                    if (!Li_bsdf.isBlack()) {
                        float lightPdfGeo = hitEmitter->pdf(isect, secIsect);
                        if (std::isinf(lightPdfGeo)) lightPdfGeo = 0.0f;

                        float p_light = sampledType == BSDF_SPECULAR ? 0.0f : lightPdfGeo * lightSelectPdf;
                        float p_bsdf = bsdfPdf;

                        float misDenom = p_bsdf + p_light;
                        if (misDenom > 1e-6f) {
                            L += f * Li_bsdf * p_bsdf / misDenom;
                        }
                    }
                }

            }

            return L;
        }

        std::string toString() const override {
            return "DirectLightIntegrator[]";
        }

    private:
    };

    GND_REGISTER_CLASS(DirectLight, "direct");
}
