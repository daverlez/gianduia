#pragma once

#include <gianduia/core/integrator.h>
#include <gianduia/math/kdtree.h>
#include <gianduia/math/photon.h>
#include "gianduia/core/factory.h"

namespace gnd {

    class PhotonMapperIntegrator : public SamplerIntegrator {
    public:
        PhotonMapperIntegrator(const PropertyList& props) : SamplerIntegrator(props) {
            // Global Map
            m_globalPhotonCount = props.getInteger("global_photons", 1000000);
            m_globalRadius = props.getFloat("global_radius", -1.0f);

            // Caustic Map
            m_causticPhotonCount = props.getInteger("caustic_photons", 500000);
            m_causticRadius = props.getFloat("caustic_radius", -1.0f);

            m_performFinalGather = props.getBoolean("final_gather", false);
        }

        void preprocess(Scene *scene) override {
            Photon::initTables();
            auto sampler = scene->getSampler();

            const Bounds3f sceneBounds = scene->getBounds();
            float sceneDiagonal = (sceneBounds.pMax - sceneBounds.pMin).length();
            if (m_globalRadius <= 0.0f)  m_globalRadius = sceneDiagonal * 0.01f;
            if (m_causticRadius <= 0.0f) m_causticRadius = sceneDiagonal * 0.002f;

            std::cout << "Shooting " << m_globalPhotonCount << " global photons..." << std::endl;
            std::vector<Photon> globalPhotons;

            float emittersCount = scene->getEmitters().size();
            m_emittedGlobalPhotons = 0;
            int storedPhotons = 0;
            int maxShootingAttempts = m_globalPhotonCount * 100;

            MemoryArena arena(262144);

            // Tracing global photons
            while (storedPhotons < m_globalPhotonCount && m_emittedGlobalPhotons < maxShootingAttempts) {
                arena.reset();

                auto emitter = scene->getRandomEmitter(sampler->next1D());
                Ray photonRay;
                float time = sampler->next1D();
                Color3f photonPower = emitter->samplePhoton(
                    sampler->next2D(), sampler->next2D(), time, photonRay)
                    * emittersCount;
                m_emittedGlobalPhotons++;

                int depth = 0;
                bool specularPath = true;

                while (true) {
                    SurfaceInteraction isect;
                    if (!scene->rayIntersect(photonRay, isect)) break;
                    isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                    if (!isect.bsdf) {
                        photonRay = isect.spawnRay(photonRay.d);
                        photonRay.time = time;
                        continue;
                    }

                    Vector3f wi = -photonRay.d;
                    BxDFType diffuseFlags = BxDFType(BSDF_DIFFUSE | BSDF_REFLECTION | BSDF_TRANSMISSION);
                    bool hasDiffuse = isect.bsdf->numComponents(diffuseFlags) > 0;

                    if (hasDiffuse && !specularPath && depth > 0) {
                        globalPhotons.emplace_back(isect.p, photonPower, wi);
                        storedPhotons++;

                        if (storedPhotons == m_globalPhotonCount) break;
                    }

                    BxDFType specularFlags = BxDFType(BSDF_SPECULAR | BSDF_GLOSSY | BSDF_REFLECTION | BSDF_TRANSMISSION);
                    specularPath &= isect.bsdf->numComponents(specularFlags) > 0;

                    Color3f prevPower = photonPower;
                    Vector3f wo;
                    float bsdfPdf;
                    photonPower *= isect.bsdf->sample(wi, &wo, sampler->next2D(), sampler->next1D(), &bsdfPdf);

                    if (depth > 2) {
                        float survivalProb = std::min(0.99f, (photonPower.luminance() / prevPower.luminance()));
                        if (survivalProb == 0.0f || sampler->next1D() > survivalProb) break;
                        photonPower /= survivalProb;
                    }
                    photonRay = isect.spawnRay(wo);
                    photonRay.time = time;
                    depth++;
                }
            }

            std::cout << "Building KD-Tree with " << globalPhotons.size() << " global photons..." << std::endl;
            m_globalMap.build(globalPhotons);
            std::cout << "Global photon Map ready." << std::endl;

            std::cout << "Shooting " << m_causticPhotonCount << " caustic photons..." << std::endl;
            std::vector<Photon> causticPhotons;

            m_emittedCausticPhotons = 0;
            storedPhotons = 0;
            maxShootingAttempts = m_causticPhotonCount * 100;

            // Tracing caustic photons
            while (storedPhotons < m_causticPhotonCount && m_emittedCausticPhotons < maxShootingAttempts) {
                arena.reset();

                auto emitter = scene->getRandomEmitter(sampler->next1D());
                Ray photonRay;
                float time = sampler->next1D();
                Color3f photonPower = emitter->samplePhoton(
                    sampler->next2D(), sampler->next2D(), time, photonRay)
                    * emittersCount;
                m_emittedCausticPhotons++;

                int depth = 0;
                bool specularPath = true;

                while (true) {
                    SurfaceInteraction isect;
                    if (!scene->rayIntersect(photonRay, isect)) break;
                    isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                    if (!isect.bsdf) {
                        photonRay = isect.spawnRay(photonRay.d);
                        photonRay.time = time;
                        continue;
                    }

                    Vector3f wi = -photonRay.d;
                    BxDFType diffuseFlags = BxDFType(BSDF_DIFFUSE | BSDF_REFLECTION | BSDF_TRANSMISSION);
                    bool hasDiffuse = isect.bsdf->numComponents(diffuseFlags) > 0;

                    if (hasDiffuse) {
                        if (specularPath && depth > 0) {
                            causticPhotons.emplace_back(isect.p, photonPower, wi);
                            storedPhotons++;
                        }
                        break;
                    }

                    BxDFType specularFlags = BxDFType(BSDF_SPECULAR | BSDF_GLOSSY | BSDF_REFLECTION | BSDF_TRANSMISSION);
                    specularPath &= isect.bsdf->numComponents(specularFlags) > 0;

                    Color3f prevPower = photonPower;
                    Vector3f wo;
                    float bsdfPdf;
                    photonPower *= isect.bsdf->sample(wi, &wo, sampler->next2D(), sampler->next1D(), &bsdfPdf);

                    if (depth > 2) {
                        float survivalProb = std::min(0.99f, (photonPower.luminance() / prevPower.luminance()));
                        if (survivalProb == 0.0f || sampler->next1D() > survivalProb) break;
                        photonPower /= survivalProb;
                    }
                    photonRay = isect.spawnRay(wo);
                    photonRay.time = time;
                    depth++;
                }
            }

            std::cout << "Building KD-Tree with " << causticPhotons.size() << " caustic photons..." << std::endl;
            m_causticMap.build(causticPhotons);
            arena.reset();

            std::cout << "Caustic photon Map ready." << std::endl;
        }

        Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena,
                   AOVRecord* aovs = nullptr) const override {
            auto powerHeuristic = [](int nf, float fPdf, int ng, float gPdf) -> float {
                float f = nf * fPdf; float g = ng * gPdf;
                float denom = (f * f) + (g * g);
                return denom > 0.0f ? (f * f) / denom : 0.0f;
            };

            Color3f L(0.0f);
            Color3f tp(1.0f);
            Ray r = ray;
            float time = r.time;

            int depth = 0;
            bool specularBounce = true;
            float pdfPrev = 1.0f;
            Point3f lastScatterPoint = r.o;
            bool firstDiffuseBounce = true;

            while (true) {
                SurfaceInteraction isect;
                if (!scene.rayIntersect(r, isect)) break;

                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                // Emitted radiance
                if (isect.primitive->getEmitter()) {
                    std::shared_ptr<Emitter> emitter = isect.primitive->getEmitter();
                    bool skipEmission = (depth == 0 && !isect.bsdf);

                    if (!skipEmission) {
                        Color3f Le = emitter->eval(isect, -r.d);
                        if (!Le.isBlack()) {
                            float weight = 1.0f;
                            if (!specularBounce) {
                                SurfaceInteraction dummyRef; dummyRef.p = lastScatterPoint;
                                float lightPdf = emitter->pdf(dummyRef, isect);
                                float p_light = lightPdf * (1.0f / scene.getEmitters().size());
                                weight = powerHeuristic(1, pdfPrev, 1, p_light);
                            } else if (!firstDiffuseBounce) {
                                weight = 0.0f;
                            }
                            L += tp * Le * weight;
                        }
                    }
                }

                if (!isect.bsdf) {
                    r = isect.spawnRay(r.d);
                    r.time = time;
                    continue;
                }

                // Next Event Estimation
                float lightSelectPdf = 1.0f / scene.getEmitters().size();
                auto emitter = scene.getRandomEmitter(sampler.next1D());

                SurfaceInteraction lightInfo;
                float lightPdf;
                Ray shadowRay;
                Color3f Ld = emitter->sample(isect, sampler.next2D(), lightInfo, lightPdf, shadowRay);
                Ld /= lightSelectPdf;

                if (!Ld.isBlack() && lightPdf > 1e-6f) {
                    bool occluded = false;
                    Ray traceRay = shadowRay;

                    while (true) {
                        SurfaceInteraction shadowIsect;
                        if (!scene.rayIntersect(traceRay, shadowIsect)) break;

                        shadowIsect.primitive->getMaterial()->computeScatteringFunctions(shadowIsect, arena);
                        if (shadowIsect.bsdf != nullptr) {
                            occluded = true;
                            break;
                        }

                        float nextTmax = traceRay.tMax - (shadowIsect.p - traceRay.o).length();
                        traceRay = shadowIsect.spawnRay(traceRay.d);
                        traceRay.tMax = nextTmax;
                        traceRay.time = shadowIsect.time;
                    }

                    if (!occluded) {
                        Vector3f wi = Normalize(lightInfo.p - isect.p);
                        float bsdfPdf = isect.bsdf->pdf(-r.d, wi);
                        Color3f f = isect.bsdf->f(-r.d, wi);

                        if (!f.isBlack() && bsdfPdf > 1e-6f) {
                            float p_light = lightPdf * lightSelectPdf;
                            float p_bsdf = emitter->isDelta() ? 0.0f : bsdfPdf;
                            float weightLight = powerHeuristic(1, p_light, 1, p_bsdf);
                            L += tp * f * Ld * std::abs(Dot(isect.n, wi)) * weightLight;
                        }
                    }
                }

                // Photon Gather & Final Gather Routing
                BxDFType diffuseFlags = BxDFType(BSDF_DIFFUSE | BSDF_REFLECTION | BSDF_TRANSMISSION);
                if (isect.bsdf->numComponents(diffuseFlags) > 0) {

                    Color3f caustics(0.0f);
                    Photon query; query.p = isect.p;
                    int foundCaustic = 0;
                    m_causticMap.searchRadius(query, m_causticRadius, [&](const Photon& photon, float dist2) {
                        Vector3f photonWi = photon.getDirection();
                        Color3f f = isect.bsdf->f(-r.d, photonWi);
                        if (!f.isBlack()) caustics += f * photon.getPower();
                        foundCaustic++;
                    });

                    if (foundCaustic > 0) {
                        float causticArea = Pi * m_causticRadius * m_causticRadius;
                        L += tp * caustics / (static_cast<float>(m_emittedCausticPhotons) * causticArea);
                    }

                    if (!firstDiffuseBounce || !m_performFinalGather) {
                        Color3f indirect(0.0f);
                        int foundGlobal = 0;
                        m_globalMap.searchRadius(query, m_globalRadius, [&](const Photon& photon, float dist2) {
                            Vector3f photonWi = photon.getDirection();
                            Color3f f = isect.bsdf->f(-r.d, photonWi);
                            if (!f.isBlack()) indirect += f * photon.getPower();
                            foundGlobal++;
                        });

                        if (foundGlobal > 0) {
                            float globalArea = Pi * m_globalRadius * m_globalRadius;
                            L += tp * indirect / (static_cast<float>(m_emittedGlobalPhotons) * globalArea);
                        }

                        break;
                    }

                    firstDiffuseBounce = false;
                }

                // Russian Roulette
                if (depth > 2) {
                    float successProb = std::max(0.05f, std::min(0.99f, tp.luminance()));
                    if (sampler.next1D() > successProb) break;
                    tp /= successProb;
                }

                // BSDF sampling
                Vector3f wi;
                BxDFType sampledType;
                Color3f f_cos = isect.bsdf->sample(-r.d, &wi, sampler.next2D(), sampler.next1D(), &pdfPrev, &sampledType);

                if (f_cos.isBlack() || pdfPrev <= 1e-6f) break;

                specularBounce = (sampledType & BSDF_SPECULAR) != 0;
                tp *= f_cos;

                lastScatterPoint = isect.p;
                r = isect.spawnRay(wi);
                r.time = time;
                depth++;
            }

            return L;
        }

        std::string toString() const override {
            return std::format(
                "PhotonMapperIntegrator[\n"
                "  global photons = {},\n"
                "  caustic photons = {},\n"
                "  global photons lookup radius = {},\n"
                "  caustic photons lookup radius = {},\n"
                "  perform final gather = {}\n"
                "]",
                m_globalPhotonCount,
                m_causticPhotonCount,
                m_globalRadius,
                m_causticRadius,
                m_performFinalGather ? "true" : "false");
        }

    private:
        int m_globalPhotonCount;
        int m_causticPhotonCount;
        float m_globalRadius;
        float m_causticRadius;

        uint64_t m_emittedGlobalPhotons;
        uint64_t m_emittedCausticPhotons;

        bool m_performFinalGather;

        KdTree<Photon> m_globalMap;
        KdTree<Photon> m_causticMap;
    };

    GND_REGISTER_CLASS(PhotonMapperIntegrator, "photonmapper");
}
