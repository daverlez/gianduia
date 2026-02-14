#pragma once
#include <gianduia/core/object.h>
#include <gianduia/core/sampler.h>
#include <gianduia/scene/scene.h>

#include "bitmap.h"

namespace gnd {

    class Integrator : public GndObject {
    public:
        virtual ~Integrator() = default;

        virtual void render(Scene *scene) = 0;

        EClassType getClassType() const override { return EIntegrator; }
    };

    class SamplerIntegrator : public Integrator {
    public:
        SamplerIntegrator(const PropertyList& props) { }

        virtual Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler) const = 0;

        void render(Scene *scene) override {
            auto camera = scene->getCamera();
            auto masterSampler = scene->getSampler();

            int width = camera->getWidth();
            int height = camera->getHeight();

            Bitmap film(width, height);

            for (int y = 0; y < height; ++y) {

                std::unique_ptr<Sampler> threadSampler = masterSampler->clone();

                for (int x = 0; x < width; ++x) {

                    uint64_t pixelIdx = y * width + x;
                    threadSampler->seed(pixelIdx);

                    Point2f pixelSample = threadSampler->next2D();
                    Point2f screenSample((x + pixelSample.x()) / width,
                                         1.0f - (y + pixelSample.y()) / height);

                    Ray ray;
                    camera->shootRay(screenSample, &ray);

                    Color3f color = Li(ray, *scene, *threadSampler);

                    if (!color.hasNaNs())
                        film.setPixel(x, y, color);
                }
            }

            film.savePNG("output.png");
        }
    };
}
