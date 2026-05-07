#pragma once
#include <gianduia/core/object.h>
#include <gianduia/core/sampler.h>
#include <gianduia/scene/scene.h>
#include <gianduia/core/film.h>

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>

#include <iostream>
#include <functional>

#include <gianduia/core/arena.h>

namespace gnd {

    class Integrator : public GndObject {
    public:
        virtual ~Integrator() = default;
        virtual void render(Scene *scene) = 0;
        virtual void cancel() { m_stopRequested = true; }
        virtual void preprocess(Scene *scene) { }

        using RenderCallback = std::function<void(int sampleIndex, const Film& film)>;
        void setCallback(RenderCallback cb) { m_callback = cb; }

        EClassType getClassType() const override { return EIntegrator; }

    protected:
        void notifyUpdate(int sampleIndex, const Film& film) {
            if (m_callback && !m_stopRequested) {
                m_callback(sampleIndex, film);
            }
        }

        std::atomic<bool> m_stopRequested = false;

        RenderCallback m_callback;
    };

    class SamplerIntegrator : public Integrator {
    public:
        SamplerIntegrator(const PropertyList& props) { }

        virtual Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena,
                            AOVRecord* aovs = nullptr) const = 0;

        void render(Scene *scene) override {
            m_stopRequested = false;

            auto camera = scene->getCamera();
            auto masterSampler = scene->getSampler();

            size_t sampleCount = masterSampler->getSampleCount();

            int width = camera->getWidth();
            int height = camera->getHeight();

            Film* film = camera->getFilm();
            film->clear();

            std::cout << "Rendering started..." << std::endl;

            struct ThreadState {
                std::unique_ptr<Sampler> sampler;
                MemoryArena arena;

                ThreadState(std::unique_ptr<Sampler> s)
                    : sampler(std::move(s)), arena(262144) {} // 256KB block size
            };

            tbb::enumerable_thread_specific<ThreadState> threadStates(
                [&]() {
                    return ThreadState(masterSampler->clone());
                }
            );

            for (size_t s = 0; s < sampleCount; ++s) {
                if (m_stopRequested) break;

                tbb::parallel_for(tbb::blocked_range<int>(0, height), [&](const tbb::blocked_range<int>& range) {

                    ThreadState& localState = threadStates.local();
                    Sampler& threadSampler = *localState.sampler;
                    MemoryArena& threadArena = localState.arena;

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

                            float channelRnd = threadSampler.next1D();
                            int channel; // 0 = R, 1 = G, 2 = B, -1 = no aberration

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

                            camSample.time = threadSampler.next1D();

                            Ray ray;
                            float rayWeight = camera->shootRay(camSample, &ray);
                            AOVRecord pixelAOVs;

                            Color3f rawColor = Li(ray, *scene, threadSampler, threadArena, &pixelAOVs);
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

                film->resolve();
                notifyUpdate(s, *film);
            }

            film->saveEXR();
            film->savePNG();
            std::cout << "Done!" << std::endl;
        }
    };
}
