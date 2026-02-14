#pragma once
#include "bvh.h"
#include <gianduia/core/camera.h>
#include <gianduia/core/object.h>
#include <gianduia/shapes/primitive.h>
#include <gianduia/core/sampler.h>

namespace gnd {

    class Integrator;

    class Scene : public GndObject {
    public:
        Scene(const PropertyList& props);

        virtual void addChild(std::shared_ptr<GndObject> child) override;
        virtual void activate() override;
        virtual std::string toString() const override;
        virtual EClassType getClassType() const override { return EScene; }

        std::shared_ptr<Camera> getCamera() { return m_camera; }
        std::shared_ptr<Sampler> getSampler() { return m_sampler; }
        std::shared_ptr<Integrator> getIntegrator() { return m_integrator; }

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect) const;
        bool rayIntersect(const Ray& ray) const;

        void render();

    private:
        std::vector<std::shared_ptr<Primitive>> m_primitives;
        std::shared_ptr<Camera> m_camera;
        BVH m_bvh;
        std::shared_ptr<Sampler> m_sampler;
        std::shared_ptr<Integrator> m_integrator;
        // std::vector<std::shared_ptr<Emitter>> m_emitters;
    };
}
