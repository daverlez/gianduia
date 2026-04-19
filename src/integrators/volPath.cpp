#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"
#include "gianduia/math/phase.h"
#include "gianduia/materials/medium.h"

namespace gnd {

    class VolumetricPathIntegrator : public SamplerIntegrator {
    public:
        VolumetricPathIntegrator(const PropertyList& props) : SamplerIntegrator(props) {
            maxDepth = props.getInteger("maxDepth", 1000);
        }

        Color3f Li(const Ray& primaryRay, Scene& scene, Sampler& sampler, MemoryArena& arena, Color3f* outAlbedo, Color3f* outNormal) const override {
            auto powerHeuristic = [](int nf, float fPdf, int ng, float gPdf) -> float {
                float f = nf * fPdf; float g = ng * gPdf;
                float denom = (f * f) + (g * g);
                return denom > 0.0f ? (f * f) / denom : 0.0f;
            };

            Color3f L(0.0f);
            Color3f tp(1.0f);
            Ray r = primaryRay;
            int bounces = 0;

            float pdfPrev = 1.0f;
            bool specularBounce = true;
            bool needsGBuffer = (outAlbedo != nullptr || outNormal != nullptr);

            while (bounces < maxDepth) {
                SurfaceInteraction isect;
                bool hitSurface = scene.rayIntersect(r, isect);

                MediumInteraction mi;
                if (r.medium) {
                    tp *= r.medium->sample(r, sampler, arena, mi);
                }
                if (tp.isBlack()) break;

                // --- Volumetric scattering ---
                if (mi.isValid()) {
                    // --- Volume NEE ---
                    float lightSelectPdf = 1.0f / scene.getEmitters().size();
                    std::shared_ptr<Emitter> emitter = scene.getRandomEmitter(sampler.next1D());

                    SurfaceInteraction mi_ref;
                    mi_ref.p = mi.p;

                    SurfaceInteraction lightIsect;
                    float lightPdf;
                    Ray shadowRay;
                    Color3f Li_nee = emitter->sample(mi_ref, sampler.next2D(), lightIsect, lightPdf, shadowRay);
                    Li_nee /= lightSelectPdf;

                    if (lightPdf > 1e-6f && !Li_nee.isBlack()) {
                        Color3f Tr = evaluateTr(scene, shadowRay, sampler, lightIsect.p, arena);

                        if (!Tr.isBlack()) {
                            Vector3f wi = Normalize(lightIsect.p - mi.p);
                            float phasePdf = mi.phase->p(mi.wo, wi);

                            if (phasePdf > 1e-6f) {
                                float p_light = lightPdf * lightSelectPdf;
                                float p_phase = emitter->isDelta() ? 0.0f : phasePdf;
                                float weightLight = powerHeuristic(1, p_light, 1, p_phase);

                                L += tp * phasePdf * Li_nee * weightLight * Tr;
                            }
                        }
                    }

                    // --- Phase Function Sampling ---
                    Vector3f wi;
                    pdfPrev = mi.phase->sample(mi.wo, &wi, sampler.next2D());
                    specularBounce = false;

                    const Medium* prevMedium = r.medium;
                    float prevTime = r.time;
                    r = Ray(mi.p, wi);
                    r.medium = prevMedium;
                    r.time = prevTime;

                    // --- Russian Roulette ---
                    if (bounces > 2) {
                        float q = std::max(0.05f, std::min(tp.luminance(), 0.99f));
                        if (sampler.next1D() > q) break;
                        tp /= q;
                    }

                    bounces++;
                    continue;
                }

                // --- Surface scattering ---
                if (!hitSurface) {
                    Color3f Li_env(0.0f);
                    if (scene.getEnvMap()) {
                        Li_env = scene.getEnvMap()->eval(SurfaceInteraction(), r.d);

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
                        if (outAlbedo) *outAlbedo = Li_env;
                        if (outNormal) *outNormal = Color3f(0.0f);
                        needsGBuffer = false;
                    }
                    break;
                }

                if (isect.primitive->getEmitter()) {
                    Color3f Le = isect.primitive->getEmitter()->eval(isect, -r.d);
                    if (!Le.isBlack()) {
                        float weight = 1.0f;
                        if (!specularBounce) {
                            SurfaceInteraction dummyRef; dummyRef.p = r.o;
                            float lightPdf = isect.primitive->getEmitter()->pdf(dummyRef, isect);
                            float p_light = lightPdf * (1.0f / scene.getEmitters().size());
                            weight = powerHeuristic(1, pdfPrev, 1, p_light);
                        }
                        L += tp * Le * weight;
                    }
                }

                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                if (needsGBuffer && isect.bsdf) {
                    if (isect.bsdf->numComponents(BxDFType(BSDF_ALL & ~BSDF_SPECULAR)) > 0 || bounces == maxDepth - 1) {
                        if (outAlbedo) *outAlbedo = isect.primitive->getMaterial()->getAlbedo(isect);
                        if (outNormal) {
                            Normal3f n = isect.n / 2.0f + Normal3f(0.5f);
                            *outNormal = Color3f(n.x(), n.y(), n.z());
                        }
                        needsGBuffer = false;
                    }
                }

                if (!isect.bsdf) {
                    r = Ray(isect.p, r.d);
                    r.medium = isect.getMedium(r.d);
                    r.time = isect.time;
                    continue;
                }

                // --- Surface NEE ---
                float lightSelectPdf = 1.0f / scene.getEmitters().size();
                std::shared_ptr<Emitter> emitter = scene.getRandomEmitter(sampler.next1D());

                SurfaceInteraction lightIsect;
                float lightPdf;
                Ray shadowRay;
                Color3f Li_nee = emitter->sample(isect, sampler.next2D(), lightIsect, lightPdf, shadowRay);
                Li_nee /= lightSelectPdf;

                if (lightPdf > 1e-6f && !Li_nee.isBlack()) {
                    Color3f Tr = evaluateTr(scene, shadowRay, sampler, lightIsect.p, arena);

                    if (!Tr.isBlack()) {
                        Vector3f wi = Normalize(lightIsect.p - isect.p);
                        float bsdfPdf = isect.bsdf->pdf(-r.d, wi);
                        Color3f f = isect.bsdf->f(-r.d, wi);

                        if (!f.isBlack() && bsdfPdf > 1e-6f) {
                            float p_light = lightPdf * lightSelectPdf;
                            float p_bsdf = emitter->isDelta() ? 0.0f : bsdfPdf;
                            float weightLight = powerHeuristic(1, p_light, 1, p_bsdf);

                            L += tp * f * Li_nee * weightLight * Tr;
                        }
                    }
                }

                // --- BSDF Sampling ---
                BxDFType sampledType;
                Vector3f wi;
                Color3f f_cos = isect.bsdf->sample(-r.d, &wi, sampler.next2D(), sampler.next1D(), &pdfPrev, &sampledType);

                if (f_cos.isBlack() || pdfPrev <= 1e-6f) break;

                specularBounce = (sampledType & BSDF_SPECULAR) != 0;
                tp *= f_cos;

                r = Ray(isect.p, wi);
                r.medium = isect.getMedium(wi);
                r.time = isect.time;

                // --- Russian Roulette ---
                if (bounces > 2) {
                    float q = std::max(0.05f, std::min(tp.luminance(), 0.99f));
                    if (sampler.next1D() > q) break;
                    tp /= q;
                }

                bounces++;
            }

            return L;
        }

        std::string toString() const override {
            return "VolumetricPathIntegrator[maxDepth=" + std::to_string(maxDepth) + "]";
        }

    private:
        int maxDepth;

        Color3f evaluateTr(Scene& scene, Ray shadowRay, Sampler& sampler, const Point3f& lightP, MemoryArena& arena) const {
            Color3f Tr(1.0f);
            Ray r = shadowRay;

            while (true) {
                SurfaceInteraction isect;
                bool hit = scene.rayIntersect(r, isect);

                if (r.medium) {
                    Tr *= r.medium->Tr(r, sampler);
                }

                if (Tr.isBlack()) return Tr;
                if (!hit) break;
                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                if (isect.bsdf != nullptr) {
                    return {0.0f};
                }

                r = Ray(isect.p, r.d);
                r.medium = isect.getMedium(r.d);
                r.tMin = Epsilon;
                r.tMax = Distance(isect.p, lightP) - Epsilon;
                r.time = isect.time;
            }

            return Tr;
        }
    };

    GND_REGISTER_CLASS(VolumetricPathIntegrator, "volpath");
}
