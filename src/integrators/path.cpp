#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"

namespace gnd {
    class PathIntegrator : public SamplerIntegrator {
    public:
        PathIntegrator(const PropertyList& props) : SamplerIntegrator(props) {
            maxDepth = props.getInteger("maxDepth", 1000);
        }

        Color3f Li(const Ray& primaryRay, Scene& scene, Sampler& sampler, MemoryArena& arena, AOVRecord* aovs = nullptr) const override {
            auto powerHeuristic = [](int nf, float fPdf, int ng, float gPdf) -> float {
                float f = nf * fPdf; float g = ng * gPdf;
                float denom = (f * f) + (g * g);
                return denom > 0.0f ? (f * f) / denom : 0.0f;
            };

            Color3f L(0.0f);
            Color3f tp(1.0f);
            Ray r = primaryRay;

            int bounces = 0;
            int nullBounces = 0;
            bool specularBounce = true;
            bool needsGBuffer = aovs != nullptr;
            float pdfPrev = 1.0f;

            Point3f lastScatterPoint = primaryRay.o;

            while (bounces < maxDepth) {
                SurfaceInteraction isect;
                bool hitSurface = scene.rayIntersect(r, isect);

                if (!hitSurface) {
                    if (bounces == 0 && aovs) {
                        aovs->depth = -1.0f;
                        aovs->metallic = 0.0f;
                        aovs->roughness = 0.0f;
                    }

                    if (scene.getEnvMap()) {
                        Color3f Li_env = scene.getEnvMap()->eval(SurfaceInteraction(), r.d);
                        if (!Li_env.isBlack()) {
                            float weight = 1.0f;
                            if (!specularBounce) {
                                SurfaceInteraction envIsect;
                                envIsect.p = r.o + r.d * 1e5f;
                                envIsect.n = Normal3f(-r.d);
                                float envPdf = scene.getEnvMap()->pdf(SurfaceInteraction(), envIsect);
                                float p_env = envPdf * (1.0f / scene.getEmitters().size());
                                weight = powerHeuristic(1, pdfPrev, 1, p_env);
                            }
                            L += tp * Li_env * weight;
                        }
                    }
                    if (needsGBuffer) {
                        if (scene.getEnvMap()) aovs->albedo = scene.getEnvMap()->eval(SurfaceInteraction(), r.d);
                        aovs->normal = Normal3f(0.0f);
                        needsGBuffer = false;
                    }
                    break;
                }

                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                // Emitted radiance at surface interaction
                if (isect.primitive->getEmitter()) {
                    std::shared_ptr<Emitter> emitter = isect.primitive->getEmitter();
                    bool skipEmission = (bounces == 0 && !isect.bsdf);

                    if (!skipEmission) {
                        Color3f Le = emitter->eval(isect, -r.d);
                        if (!Le.isBlack()) {
                            float weight = 1.0f;
                            if (!specularBounce) {
                                SurfaceInteraction dummyRef; dummyRef.p = lastScatterPoint;
                                float lightPdf = emitter->pdf(dummyRef, isect);
                                float p_light = lightPdf * (1.0f / scene.getEmitters().size());
                                weight = powerHeuristic(1, pdfPrev, 1, p_light);
                            }
                            L += tp * Le * weight;
                        }
                    }
                }

                if (!isect.bsdf) {
                    if (nullBounces++ > 100) break;
                    r = Ray(isect.p, r.d);
                    r.time = isect.time;
                    continue;
                }

                if (bounces == 0 && aovs) {
                    aovs->depth = Dot(isect.p - primaryRay.o, scene.getCamera()->getForward());
                    aovs->metallic = isect.primitive->getMaterial()->getMetallic(isect);
                    aovs->roughness = isect.primitive->getMaterial()->getRoughness(isect);
                }

                if (needsGBuffer) {
                    if (isect.bsdf->numComponents(BxDFType(BSDF_ALL & ~BSDF_SPECULAR)) > 0 || bounces == maxDepth - 1) {
                        aovs->albedo = isect.primitive->getMaterial()->getAlbedo(isect);
                        aovs->normal = isect.n / 2.0f + Normal3f(0.5f);
                        needsGBuffer = false;
                    }
                }

                // Next Event Estimation
                float lightSelectPdf = 1.0f / scene.getEmitters().size();
                std::shared_ptr<Emitter> emitter = scene.getRandomEmitter(sampler.next1D());

                SurfaceInteraction lightIsect;
                float lightPdf;
                Ray shadowRay;

                Color3f Li_nee = emitter->sample(isect, sampler.next2D(), lightIsect, lightPdf, shadowRay);
                Li_nee /= lightSelectPdf;

                if (lightPdf > 1e-6f && !Li_nee.isBlack()) {
                    // Passthrough shadows on primitives with null bsdf
                    bool occluded = false;
                    Ray traceRay = shadowRay;

                    while (true) {
                        SurfaceInteraction shadowIsect;
                        if (!scene.rayIntersect(traceRay, shadowIsect)) {
                            break;
                        }

                        shadowIsect.primitive->getMaterial()->computeScatteringFunctions(shadowIsect, arena);
                        if (shadowIsect.bsdf != nullptr) {
                            occluded = true;
                            break;
                        }

                        float nextTmax = traceRay.tMax - (shadowIsect.p - traceRay.o).length();
                        traceRay = Ray(shadowIsect.p, traceRay.d);
                        traceRay.tMax = nextTmax;
                        traceRay.time = shadowIsect.time;
                    }

                    if (!occluded) {
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
                }

                // BSDF Sampling
                Vector3f wi;
                BxDFType sampledType;

                Color3f f_cos = isect.bsdf->sample(-r.d, &wi, sampler.next2D(), sampler.next1D(), &pdfPrev, &sampledType);

                if (f_cos.isBlack() || pdfPrev <= 1e-6f) break;

                specularBounce = (sampledType & BSDF_SPECULAR) != 0;
                tp *= f_cos;

                // Russian Roulette
                if (bounces > 2) {
                    float q = std::max(0.05f, std::min(tp.luminance(), 0.99f));
                    if (sampler.next1D() > q) break;
                    tp /= q;
                }

                lastScatterPoint = isect.p;
                r = Ray(isect.p, wi);
                r.time = isect.time;
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