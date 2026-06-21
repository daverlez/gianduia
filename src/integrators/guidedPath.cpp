#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/stree.h"
#include "gianduia/math/warp.h"

namespace gnd {

    struct SpatialRecord {
        Point3f p;
        Vector3f dir;
        float radiance;
    };

    class GuidedPathIntegrator : public Integrator {
    public:
        GuidedPathIntegrator(const PropertyList& props) {
            maxDepth = props.getInteger("maxDepth", 1000);
            spatialThreshold = props.getInteger("spatialThreshold", 4000);
            maxTrainingPasses = props.getInteger("maxTrainingPasses", 16); 
        }

        void render(Scene* scene) override {
            m_stopRequested = false;

            auto camera = scene->getCamera();
            auto masterSampler = scene->getSampler();

            size_t sampleCount = masterSampler->getSampleCount();

            int width = camera->getWidth();
            int height = camera->getHeight();

            Film* film = camera->getFilm();
            film->clear();

            std::cout << "Rendering started..." << std::endl;

            STree sTree(scene->getBounds());

            struct GuidedThreadState {
                std::unique_ptr<Sampler> sampler;
                MemoryArena arena;
                std::vector<SpatialRecord> spatialRecords;

                GuidedThreadState(std::unique_ptr<Sampler> s)
                    : sampler(std::move(s)), arena(262144) {
                    spatialRecords.reserve(10000);
                }
            };

            tbb::enumerable_thread_specific<GuidedThreadState> threadStates(
                [&]() { return GuidedThreadState(masterSampler->clone()); }
            );

            for (size_t s = 0; s < sampleCount; ++s) {
                if (m_stopRequested) break;

                bool isTrainingPass = (s < maxTrainingPasses);

                tbb::parallel_for(tbb::blocked_range<int>(0, height), [&](const tbb::blocked_range<int>& range) {

                    GuidedThreadState& localState = threadStates.local();
                    Sampler& threadSampler = *localState.sampler;
                    MemoryArena& threadArena = localState.arena;
                    std::vector<SpatialRecord>& localRecords = localState.spatialRecords;

                    for (int y = range.begin(); y != range.end(); ++y) {
                        for (int x = 0; x < width; ++x) {
                            threadArena.reset();

                            uint64_t pixelIdx = y * width + x;
                            uint64_t globalIdx = pixelIdx + s * (width * height);
                            threadSampler.seed(globalIdx);

                            CameraSample camSample;

                            Point2f pixelSample = threadSampler.next2D();
                            Point2f pFilm(x + pixelSample.x(), y + pixelSample.y());
                            camSample.pFilm = Point2f(pFilm.x() / width, 1.0f - pFilm.y() / height);
                            camSample.pLens = threadSampler.next2D();
                            camSample.time = threadSampler.next1D();

                            float channelRnd = threadSampler.next1D();
                            int channel;

                            if (!camera->hasChromaticAberration()) {
                                channel = -1;
                                camSample.lambdaOffset = 0.0f;
                            }
                            else if (channelRnd < 0.333333f) {
                                channel = 0;
                                camSample.lambdaOffset = 1.0f;
                            } else if (channelRnd < 0.666666f) {
                                channel = 1;
                                camSample.lambdaOffset = 0.0f;
                            } else {
                                channel = 2;
                                camSample.lambdaOffset = -1.0f;
                            }

                            Ray ray;
                            float rayWeight = camera->shootRay(camSample, &ray);
                            AOVRecord pixelAOVs;

                            Color3f rawColor = Li(ray, *scene, threadSampler, threadArena, &pixelAOVs, sTree, localRecords, isTrainingPass);
                            rawColor *= rayWeight;

                            Color3f newColor(0.0f);
                            if (channel == -1) newColor = rawColor;
                            else if (channel == 0) newColor.r() = rawColor.r() * 3.0f;
                            else if (channel == 1) newColor.g() = rawColor.g() * 3.0f;
                            else newColor.b() = rawColor.b() * 3.0f;

                            if (newColor.hasNaNs()) {
                                std::cerr << "Warning: NaN value detected at pixel (" <<
                                    x << ", " << y << ")!" << std::endl;
                                newColor = Color3f(0.0f);
                            }

                            film->addSample(pFilm, newColor, pixelAOVs);
                        }
                    }
                });

                if (isTrainingPass) {
                    for (auto& state : threadStates) {
                        for (const auto& record : state.spatialRecords) {
                            sTree.addSample(record.p, record.radiance);
                            if (DTree* dtree = sTree.getDTree(record.p))
                                dtree->addSample(record.dir, record.radiance);
                        }
                        state.spatialRecords.clear();
                    }

                    sTree.refine(spatialThreshold);
                    sTree.clearAccumulators();
                }

                film->resolve();
                notifyUpdate(s, *film);
            }

            film->saveEXR();
            film->savePNG();
            std::cout << "Done!" << std::endl;
        }

        std::string toString() const override {
            return "GuidedPathIntegrator[maxDepth=" + std::to_string(maxDepth) + "]";
        }

    private:
        int maxDepth;
        int spatialThreshold;
        int maxTrainingPasses;

        Color3f Li(const Ray& primaryRay, Scene& scene, Sampler& sampler, MemoryArena& arena, AOVRecord* aovs,
                   const STree& sTree, std::vector<SpatialRecord>& localRecords, bool isTraining) const {
            auto powerHeuristic = [](int nf, float fPdf, int ng, float gPdf) -> float {
                float f = nf * fPdf; float g = ng * gPdf;
                float denom = (f * f) + (g * g);
                return denom > 0.0f ? (f * f) / denom : 0.0f;
            };

            Color3f L(0.0f);
            Color3f tp(1.0f);
            Ray r = primaryRay;
            int bounces = 0;
            int surfaceBounces = 0;
            int volumeBounces = 0;
            int nullBounces = 0;

            float pdfPrev = 1.0f;
            bool specularBounce = true;
            bool needsGBuffer = aovs != nullptr;

            Point3f lastScatterPoint = primaryRay.o;

            float baseRadiance = std::max(sTree.estimateRadiance(primaryRay.o), 1e-4f);

            struct PathVertex {
                Point3f p;
                Vector3f wi;
                Color3f throughputToHere;
                Color3f L_accum_before;
            };
            std::vector<PathVertex> pathVertices;

            if (isTraining)
                pathVertices.push_back({primaryRay.o, primaryRay.d, tp, L});

            while (bounces < maxDepth) {
                SurfaceInteraction isect;
                bool hitSurface = scene.rayIntersect(r, isect);

                MediumInteraction mi;
                if (r.medium) {
                    tp *= r.medium->sample(r, sampler, arena, mi);
                }

                // Volumetric scattering
                if (mi.isValid()) {
                    if (isTraining)
                        pathVertices.push_back({mi.p, Vector3f(1.0f), tp, L});

                    // Volumetric emission
                    Color3f emission = mi.medium->Le(mi.p);
                    if (!emission.isBlack()) {
                        L += tp * mi.sigma_a * emission;
                    }

                    tp *= mi.sigma_s;
                    if (tp.isBlack()) break;

                    // Volume NEE
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
                        shadowRay.medium = mi.medium;
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

                    // Phase Function Sampling
                    Vector3f wi;
                    pdfPrev = mi.phase->sample(mi.wo, &wi, sampler.next2D());
                    specularBounce = false;

                    if (isTraining)
                        pathVertices.back().wi = wi;

                    const Medium* prevMedium = r.medium;
                    float prevTime = r.time;

                    lastScatterPoint = mi.p;

                    r = Ray(mi.p, wi);
                    r.medium = prevMedium;
                    r.time = prevTime;

                    // Russian Roulette (relaxed on volumes)
                    if (volumeBounces > 6) {
                        float q = std::max(0.05f, std::min(tp.luminance(), 0.99f));
                        if (sampler.next1D() > q) break;
                        tp /= q;
                    }

                    bounces++;
                    volumeBounces++;
                    continue;
                }

                // Surface scattering
                const Medium* currentRayMedium = r.medium;

                if (!hitSurface) {
                    if (bounces == 0 && aovs) {
                        aovs->depth = -1.0f;
                        aovs->metallic = 0.0f;
                        aovs->roughness = 0.0f;
                    }

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
                        aovs->albedo = Li_env;
                        aovs->normal = Normal3f(0.0f);
                        needsGBuffer = false;
                    }
                    break;
                }

                if (isTraining)
                    pathVertices.push_back({isect.p, Vector3f(1.0f), tp, L});

                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);

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
                    r.medium = isect.getMedium(r.d);
                    r.time = isect.time;
                    continue;
                }

                if (bounces == 0 && aovs) {
                    aovs->depth = Dot(isect.p - primaryRay.o, scene.getCamera()->getForward());
                    aovs->metallic = isect.primitive->getMaterial()->getMetallic(isect);
                    aovs->roughness = isect.primitive->getMaterial()->getRoughness(isect);
                }

                if (needsGBuffer) {
                    if (isect.bsdf && isect.bsdf->numComponents(BxDFType(BSDF_ALL & ~BSDF_SPECULAR)) > 0 || bounces == maxDepth - 1) {
                        aovs->albedo = isect.primitive->getMaterial()->getAlbedo(isect);
                        aovs->normal = isect.n / 2.0f + Normal3f(0.5f);
                        needsGBuffer = false;
                    }
                }

                // Surface NEE
                float lightSelectPdf = 1.0f / scene.getEmitters().size();
                std::shared_ptr<Emitter> emitter = scene.getRandomEmitter(sampler.next1D());

                SurfaceInteraction lightIsect;
                float lightPdf;
                Ray shadowRay;
                Color3f Li_nee = emitter->sample(isect, sampler.next2D(), lightIsect, lightPdf, shadowRay);
                Li_nee /= lightSelectPdf;

                if (lightPdf > 1e-6f && !Li_nee.isBlack()) {
                    shadowRay.medium = getNextMedium(-r.d, shadowRay.d, currentRayMedium, isect);
                    Color3f Tr = evaluateTr(scene, shadowRay, sampler, lightIsect.p, arena);

                    if (!Tr.isBlack()) {
                        Vector3f wi = Normalize(lightIsect.p - isect.p);
                        float bsdfPdf = isect.bsdf->pdf(-r.d, wi);
                        Color3f f = isect.bsdf->f(-r.d, wi);

                        if (!f.isBlack() && bsdfPdf > 1e-6f) {
                            float p_light = lightPdf * lightSelectPdf;
                            float p_bsdf = emitter->isDelta() ? 0.0f : bsdfPdf;
                            float weightLight = powerHeuristic(1, p_light, 1, p_bsdf);

                            L += tp * f * Li_nee * std::abs(Dot(isect.n, wi)) * weightLight * Tr;
                        }
                    }
                }

                // Path Guiding (Directional Sampling via MIS)
                const DTree* dTree = sTree.getDTree(isect.p);
                float guidingFraction = (dTree && dTree->getStatisticalWeight() > 0.0f) ? 0.5f : 0.0f;
                float bsdfFraction = 1.0f - guidingFraction;

                BxDFType sampledType;
                Vector3f wi;
                Color3f f_cos(0.0f);
                float combinedPdf = 0.0f;

                if (sampler.next1D() < guidingFraction) {
                    // Guiding with D-Tree
                    float dTreePdf;
                    wi = dTree->sample(sampler.next2D(), dTreePdf);

                    f_cos = isect.bsdf->f(-r.d, wi) * std::abs(Dot(isect.n, wi));
                    float bsdfPdf = isect.bsdf->pdf(-r.d, wi);
                    if (f_cos.isBlack() || bsdfPdf < Epsilon) break;

                    combinedPdf = guidingFraction * dTreePdf + bsdfFraction * bsdfPdf;
                    sampledType = BxDFType(BSDF_GLOSSY | BSDF_DIFFUSE);
                } else {
                    // BSDF sampling
                    float bsdfPdf;
                    Color3f bsdfSampleWeight = isect.bsdf->sample(-r.d, &wi, sampler.next2D(), sampler.next1D(), &bsdfPdf, &sampledType);

                    if (bsdfSampleWeight.isBlack() || bsdfPdf <= 1e-6f) break;

                    float dTreePdf = 0.0f;
                    bool isSpecular = (sampledType & BSDF_SPECULAR) != 0;

                    if (guidingFraction > 0.0f && !isSpecular) {
                        dTreePdf = dTree->pdf(wi);
                    }

                    if (isSpecular) {
                        combinedPdf = bsdfPdf;
                        f_cos = bsdfSampleWeight * bsdfPdf;
                    } else {
                        f_cos = isect.bsdf->f(-r.d, wi) * std::abs(Dot(isect.n, wi));
                        combinedPdf = guidingFraction * dTreePdf + bsdfFraction * bsdfPdf;
                    }
                }

                if (combinedPdf <= Epsilon) break;
                tp *= f_cos / combinedPdf;
                specularBounce = (sampledType & BSDF_SPECULAR) != 0;

                if (isTraining)
                    pathVertices.back().wi = wi;

                Vector3f incidentDir = -r.d;
                lastScatterPoint = isect.p;

                r = Ray(isect.p, wi);
                r.medium = getNextMedium(incidentDir, wi, currentRayMedium, isect);
                r.time = isect.time;

                // Adjoint-Driven Russian Roulette
                if (surfaceBounces > 2) {
                    float estimatedRadiance = sTree.estimateRadiance(isect.p);
                    float expectedContribution = tp.luminance() * estimatedRadiance;

                    float p_classic = std::clamp(tp.luminance(), 0.05f, 0.95f);
                    float p_adrr = std::clamp(expectedContribution / baseRadiance, 0.05f, 0.95f);
                    
                    float q = std::max(p_classic, p_adrr);

                    if (sampler.next1D() > q) break;
                    tp /= q;
                }

                bounces++;
                surfaceBounces++;
            }

            // Splatting
            if (isTraining) {
                for (const auto& vertex : pathVertices) {
                    Color3f suffixL = L - vertex.L_accum_before;
                    float tpLum = vertex.throughputToHere.luminance();

                    if (tpLum > 1e-8f) {
                        float radianceAtVertex = suffixL.luminance() / tpLum;
                        if (!std::isnan(radianceAtVertex) && !std::isinf(radianceAtVertex) && radianceAtVertex >= 0.0f)
                            localRecords.push_back({vertex.p, vertex.wi, radianceAtVertex});
                    }
                }
            }

            return L;
        }

        Color3f evaluateTr(Scene& scene, Ray shadowRay, Sampler& sampler, const Point3f& lightP, MemoryArena& arena) const {
            Color3f Tr(1.0f);
            Ray r = shadowRay;

            while (true) {
                SurfaceInteraction isect;
                bool hit = scene.rayIntersect(r, isect);

                if (r.medium) {
                    Ray trRay = r;
                    trRay.tMax = hit ? isect.t : r.tMax;
                    Tr *= r.medium->Tr(trRay, sampler);
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

        const Medium* getNextMedium(const Vector3f& wo, const Vector3f& wi, const Medium* currentRayMedium, const SurfaceInteraction& isect) const {
            if (Dot(wi, isect.n) > 0.0f)
                return Dot(wo, isect.n) > 0.0f ? currentRayMedium : isect.mediumInterface.outside;

            return Dot(wo, isect.n) > 0.0f ? isect.mediumInterface.inside : currentRayMedium;
        }
    };

    GND_REGISTER_CLASS(GuidedPathIntegrator, "guidedpath");
}