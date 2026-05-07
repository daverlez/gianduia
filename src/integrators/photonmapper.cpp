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
            Color3f L(0.0f);
            Color3f tp(1.0f);
            Ray r = ray;
            float time = r.time;
            float depth = 0;

            while (true) {
                SurfaceInteraction isect;
                if (!scene.rayIntersect(r, isect)) break;

                if (isect.primitive->getEmitter())
                    L += isect.primitive->getEmitter()->eval(isect, -r.d);

                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);
                if (!isect.bsdf) {
                    r = isect.spawnRay(r.d);
                    r.time = time;
                    continue;
                }

                // Next event estimation
                // Emitter sampling
                auto emitter = scene.getRandomEmitter(sampler.next1D());
                SurfaceInteraction lightInfo;
                float lightPdf;
                Ray shadowRay;
                Color3f Ld = emitter->sample(isect, sampler.next2D(), lightInfo, lightPdf, shadowRay);
                Ld *= scene.getEmitters().size();

                if (!Ld.isBlack() && lightPdf > 0.0f) {
                    Vector3f wi = shadowRay.d;
                    float bsdfPdf = isect.bsdf->pdf(-r.d, wi);
                    Color3f f = isect.bsdf->f(-r.d, wi);

                    if (!f.isBlack() && bsdfPdf > 0.0f) {
                        if (!scene.rayIntersect(shadowRay)) {
                            float weight = 1.0f;
                            L += tp * f * Ld * std::abs(Dot(isect.n, wi)) * weight;
                        }
                    }
                }


                // Diffuse surface: photon gather
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
                        L += tp * indirect / (m_emittedPhotons * area);
                    }

                    break;
                }

                // Non-diffuse surface: bouncing
                if (depth > 2) {
                    float successProb = std::min(0.99f, tp.luminance());
                    if (sampler.next1D() > successProb) break;
                    tp /= successProb;
                }

                Vector3f wi;
                float bsdfPdf;
                tp *= isect.bsdf->sample(-r.d, &wi, sampler.next2D(), sampler.next1D(), &bsdfPdf);
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
