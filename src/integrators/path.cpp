#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"

namespace gnd {
    class PathIntegrator : public SamplerIntegrator {
    public:
        PathIntegrator(const PropertyList& props) : SamplerIntegrator(props) { }

        Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena) const override {
            SurfaceInteraction isect;
            Color3f L(0.0f);
            float neeWeight = 1.0f;
            float matsWeight = 1.0f;
            Color3f tp = Color3f(1.0f);
            Ray r = ray;

            while (scene.rayIntersect(r, isect)) {
                if (isect.primitive->getEmitter()) {
                    L += tp * matsWeight * isect.primitive->getEmitter()->eval(isect, -r.d);
                }

                // Russian roulette
                float q = std::min(tp.luminance(), 0.99f);
                if (sampler.next1D() > q)
                    break;
                tp /= q;

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
                    float bsdfPdf = isect.bsdf->pdf(-r.d, wi);
                    Color3f f = isect.bsdf->f(-r.d, wi);

                    if (!f.isBlack() && bsdfPdf > 1e-6f) {
                        float p_light = lightPdf * lightSelectPdf;
                        float p_bsdf = emitter->isDelta() ? 0.0f : bsdfPdf;

                        float misDenom = p_light + p_bsdf;
                        if (misDenom > 1e-6f) {
                            L += tp * f * Li_nee * std::abs(Dot(isect.n, wi)) * p_light / misDenom;
                        }
                    }
                }

                // BSDF Sampling
                Vector3f wi;
                float bsdfPdf;
                BxDFType sampledType;
                tp *= isect.bsdf->sample(-r.d, &wi, sampler.next2D(), &bsdfPdf, &sampledType);

                if (!tp.isBlack() && bsdfPdf > 1e-6f) {
                    Ray secRay(isect.p, wi);
                    SurfaceInteraction secIsect;

                    if (scene.rayIntersect(secRay, secIsect) && secIsect.primitive->getEmitter()) {
                        std::shared_ptr<Emitter> hitEmitter = secIsect.primitive->getEmitter();
                        Color3f Li_bsdf = hitEmitter->eval(secIsect, -wi);

                        if (!Li_bsdf.isBlack()) {
                            float lightPdfGeo = hitEmitter->pdf(isect, secIsect);
                            if (std::isinf(lightPdfGeo)) lightPdfGeo = 0.0f;

                            float p_light = (sampledType & BSDF_SPECULAR) ? 0.0f : lightPdfGeo * lightSelectPdf;
                            float p_bsdf = bsdfPdf;

                            float misDenom = p_bsdf + p_light;
                            matsWeight = p_bsdf / misDenom;
                        }
                    }
                }
                r = Ray(isect.p, wi);
            }

            return L;
        }

        std::string toString() const override {
            return "PathIntegrator[]";
        }
    };

    GND_REGISTER_CLASS(PathIntegrator, "path");
}
