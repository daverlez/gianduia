#pragma once
#include <gianduia/core/object.h>
#include <gianduia/core/sampler.h>
#include <gianduia/scene/scene.h>
#include <gianduia/core/bitmap.h>

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

        using RenderCallback = std::function<void(int sampleIndex, const Bitmap& film)>;
        void setCallback(RenderCallback cb) { m_callback = cb; }

        EClassType getClassType() const override { return EIntegrator; }

    protected:
        void notifyUpdate(int sampleIndex, const Bitmap& film) {
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

        virtual Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena) const = 0;

        void render(Scene *scene) override {
            m_stopRequested = false;

            auto camera = scene->getCamera();
            auto masterSampler = scene->getSampler();

            size_t sampleCount = masterSampler->getSampleCount();

            int width = camera->getWidth();
            int height = camera->getHeight();

            Bitmap* film = camera->getFilm();
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

                            // Pixel jittering
                            Point2f pixelSample = threadSampler.next2D();
                            Point2f screenSample((x + pixelSample.x()) / width,
                                                 1.0f - (y + pixelSample.y()) / height);

                            Ray ray;
                            camera->shootRay(screenSample, &ray);

                            Color3f newColor = Li(ray, *scene, threadSampler, threadArena);

                            if (newColor.hasNaNs()) {
                                std::cerr << "Warning: NaN value " << newColor << " detected at pixel (" <<
                                    x << ", " << y << ")!" << std::endl;
                                newColor = Color3f(0.0f);
                            }

                            // Avg_n = Avg_{n-1} + (Val_n - Avg_{n-1}) / n
                            Color3f oldColor = film->getPixel(x, y);
                            float weight = 1.0f / (float)(s + 1);
                            Color3f blendedColor = oldColor + (newColor - oldColor) * weight;
                            film->setPixel(x, y, blendedColor);
                        }
                    }
                });

                notifyUpdate(s, *film);
            }

            film->savePNG();
            std::cout << "Done!" << std::endl;
        }
    };
}
