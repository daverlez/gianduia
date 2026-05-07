#pragma once

#include <gianduia/core/integrator.h>
#include <gianduia/math/kdtree.h>
#include <gianduia/math/photon.h>
#include "gianduia/core/factory.h"

namespace gnd {

    class PhotonMapperIntegrator : public SamplerIntegrator {
    public:
        PhotonMapperIntegrator(const PropertyList& props) : SamplerIntegrator(props) {
            m_photonCount = props.getInteger("photons", 1000000);
            m_lookupRadius = props.getFloat("lookup_radius", 0.1f);
        }

        void preprocess(Scene *scene) override {
            Photon::initTables();
            auto sampler = scene->getSampler();
            std::cout << "Shooting " << m_photonCount << " photons..." << std::endl;
            
            std::vector<Photon> globalPhotons;

            float emittersCount = scene->getEmitters().size();
            m_emittedPhotons = 0;
            int storedPhotons = 0;
            int maxShootingAttempts = m_photonCount * 100;

            MemoryArena arena(262144);

            while (storedPhotons < m_photonCount && m_emittedPhotons < maxShootingAttempts) {
                arena.reset();

                if (storedPhotons > 0 && storedPhotons % 10000 == 0 )
                    std::cout << storedPhotons << "/" << m_photonCount << std::endl;

                auto emitter = scene->getRandomEmitter(sampler->next1D());
                Ray photonRay;
                Color3f photonPower = emitter->samplePhoton(
                    sampler->next2D(), sampler->next2D(), sampler->next1D(), photonRay)
                    * emittersCount;
                m_emittedPhotons++;

                int depth = 0;
                bool specularPath = true;

                while (true) {
                    SurfaceInteraction isect;
                    if (!scene->rayIntersect(photonRay, isect)) break;

                    float time = photonRay.time;
                    isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

                    if (!isect.bsdf) {
                        photonRay = isect.spawnRay(photonRay.d);
                        photonRay.time = time;
                        continue;
                    }

                    Vector3f wi = -photonRay.d;
                    BxDFType diffuseFlags = BxDFType(BSDF_DIFFUSE | BSDF_REFLECTION | BSDF_TRANSMISSION);
                    bool hasDiffuse = isect.bsdf->numComponents(diffuseFlags) > 0;

                    if (hasDiffuse && (!specularPath || depth > 0)) {
                        globalPhotons.emplace_back(isect.p, photonPower, wi);
                        storedPhotons++;

                        if (storedPhotons == m_photonCount) break;
                    }

                    BxDFType specularFlags = BxDFType(BSDF_SPECULAR | BSDF_REFLECTION | BSDF_TRANSMISSION);
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

            std::cout << "Building KD-Tree with " << globalPhotons.size() << " photons..." << std::endl;
            m_photonMap.build(globalPhotons);
            arena.reset();
            
            std::cout << "Photon Map ready." << std::endl;
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

                // Photon Gather
                BxDFType diffuseFlags = BxDFType(BSDF_DIFFUSE | BSDF_REFLECTION | BSDF_TRANSMISSION);
                if (isect.bsdf->numComponents(diffuseFlags) > 0) {
                    Color3f indirect(0.0f);
                    int foundPhotons = 0;
                    float radius2 = m_lookupRadius * m_lookupRadius;
                    Photon query;
                    query.p = isect.p;

                    m_photonMap.searchRadius(query, m_lookupRadius, [&](const Photon& photon, float dist2) {
                        Vector3f wi = photon.getDirection();
                        Color3f f = isect.bsdf->f(-r.d, wi);
                        if (!f.isBlack()) indirect += f * photon.getPower();
                        foundPhotons++;
                    });

                    if (foundPhotons > 0) {
                        float area = Pi * radius2;
                        L += tp * indirect / (static_cast<float>(m_emittedPhotons) * area);
                    }

                    break;
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
                "  photons = {},\n"
                "  lookup_radius = {}\n"
                "]",
                m_photonCount, m_lookupRadius);
        }

    private:
        int m_photonCount;          // Photons stored in the photon map
        int m_emittedPhotons;       // Amount of photons shot from emitters
        float m_lookupRadius;

        KdTree<Photon> m_photonMap;
    };

    GND_REGISTER_CLASS(PhotonMapperIntegrator, "photonmapper");
}
