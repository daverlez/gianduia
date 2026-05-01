#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"

namespace gnd {
    class PathIntegrator : public SamplerIntegrator {
    public:
        PathIntegrator(const PropertyList& props) : SamplerIntegrator(props) {
            maxDepth = props.getInteger("maxDepth", 1000);
        }

        Color3f Li(const Ray& primaryRay, Scene& scene, Sampler& sampler, MemoryArena& arena,
            AOVRecord* aovs = nullptr) const override {
            auto powerHeuristic = [](int nf, float fPdf, int ng, float gPdf) -> float {
                float f = nf * fPdf; float g = ng * gPdf;
                float denom = (f * f) + (g * g);
                return denom > 0.0f ? (f * f) / denom : 0.0f;
            };

            SurfaceInteraction primaryIsect;
            Color3f L(0.0f);

            bool needsGBuffer = aovs != nullptr;

            if (!scene.rayIntersect(primaryRay, primaryIsect)) {
                if (scene.getEnvMap()) {
                    L += scene.getEnvMap()->eval(SurfaceInteraction(), primaryRay.d);

                    if (needsGBuffer) {
                        aovs->albedo = L;
                        aovs->normal = Normal3f(0.0f);
                        aovs->depth = -1.0f;
                        aovs->roughness = 0.0f;
                        aovs->metallic = 0.0f;
                        needsGBuffer = false;
                    }
                }
                return L;
            }

            if (aovs) {
                aovs->depth = primaryIsect.t * Dot(primaryRay.d, scene.getCamera()->getForward());
                aovs->roughness = primaryIsect.primitive->getMaterial()->getRoughness(primaryIsect);
                aovs->metallic = primaryIsect.primitive->getMaterial()->getMetallic(primaryIsect);
            }

            if (primaryIsect.primitive->getEmitter()) {
                L += primaryIsect.primitive->getEmitter()->eval(primaryIsect, -primaryRay.d);
            }

            primaryIsect.primitive->getMaterial()->computeScatteringFunctions(primaryIsect, arena);

            if (needsGBuffer) {
                if (primaryIsect.bsdf && primaryIsect.bsdf->numComponents(BxDFType(BSDF_ALL & ~BSDF_SPECULAR)) > 0) {
                    aovs->albedo = primaryIsect.primitive->getMaterial()->getAlbedo(primaryIsect);
                    aovs->normal = primaryIsect.n / 2.0f + Normal3f(0.5f);
                    needsGBuffer = false;
                }
            }

            if (!primaryIsect.bsdf) return L;

            Color3f tp(1.0f);
            float matsWeight = 1.0f;
            int bounces = 0;

            SurfaceInteraction isect = primaryIsect;
            Ray r = primaryRay;

            while (bounces < maxDepth) {

                // --- NEE (Next Event Estimation) ---
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
                        float weightLight = powerHeuristic(1, p_light, 1, p_bsdf);
                        L += tp * f * Li_nee * std::abs(Dot(isect.n, wi)) * weightLight;
                    }
                }

                // --- BSDF Sampling ---
                Vector3f wi;
                float bsdfPdf;
                BxDFType sampledType;

                tp *= isect.bsdf->sample(-r.d, &wi, sampler.next2D(), sampler.next1D(), &bsdfPdf, &sampledType);

                if (tp.isBlack() || bsdfPdf <= 1e-6f) break;

                Ray secRay(isect.p, wi);
                secRay.time = r.time;
                SurfaceInteraction secIsect;

                if (!scene.rayIntersect(secRay, secIsect)) {
                    if (scene.getEnvMap()) {
                        std::shared_ptr<Emitter> envMap = scene.getEnvMap();
                        Color3f Li_env = envMap->eval(isect, wi);

                        if (!Li_env.isBlack()) {
                            SurfaceInteraction envIsect;
                            envIsect.p = isect.p + wi * 1e5f;
                            envIsect.n = Normal3f(-wi);

                            float lightPdfGeo = envMap->pdf(isect, envIsect);
                            if (std::isinf(lightPdfGeo)) lightPdfGeo = 0.0f;

                            float p_light = (sampledType & BSDF_SPECULAR) ? 0.0f : lightPdfGeo * lightSelectPdf;
                            matsWeight = powerHeuristic(1, bsdfPdf, 1, p_light);

                            L += tp * matsWeight * Li_env;
                        }

                        if (needsGBuffer) {
                            aovs->albedo = Li_env;
                            aovs->normal = Normal3f(0.0f);
                            needsGBuffer = false;
                        }
                    }
                    break;
                }

                if (secIsect.primitive->getEmitter()) {
                    std::shared_ptr<Emitter> hitEmitter = secIsect.primitive->getEmitter();
                    Color3f Li_bsdf = hitEmitter->eval(secIsect, -wi);

                    if (!Li_bsdf.isBlack()) {
                        float lightPdfGeo = hitEmitter->pdf(isect, secIsect);
                        if (std::isinf(lightPdfGeo)) lightPdfGeo = 0.0f;
                        float p_light = (sampledType & BSDF_SPECULAR) ? 0.0f : lightPdfGeo * lightSelectPdf;
                        matsWeight = powerHeuristic(1, bsdfPdf, 1, p_light);

                        L += tp * matsWeight * Li_bsdf;
                    }
                } else {
                    matsWeight = 1.0f;
                }

                // --- Russian Roulette ---
                if (bounces > 2) {
                    float q = std::max(0.05f, std::min(tp.luminance(), 0.99f));
                    if (sampler.next1D() > q) break;
                    tp /= q;
                }

                isect = secIsect;
                r = secRay;
                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                if (needsGBuffer && isect.bsdf) {
                    if (isect.bsdf->numComponents(BxDFType(BSDF_ALL & ~BSDF_SPECULAR)) > 0 || bounces == maxDepth - 1) {
                        aovs->albedo = isect.primitive->getMaterial()->getAlbedo(isect);
                        aovs->normal = isect.n / 2.0f + Normal3f(0.5f);
                        needsGBuffer = false;
                    }
                }

                if (!isect.bsdf) break;

                bounces++;
            }

            return L;
        }

        std::string toString() const override {
            return "PathIntegrator[maxDepth=" + std::to_string(maxDepth) + "]";
        }

    private:
        int maxDepth;
    };

    GND_REGISTER_CLASS(PathIntegrator, "path");
}