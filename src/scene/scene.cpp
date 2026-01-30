#include "gianduia/scene/scene.h"

#include "gianduia/core/factory.h"

namespace gnd {
    Scene::Scene(const PropertyList& props) { }

    void Scene::addChild(std::shared_ptr<GndObject> child) {
        switch (child->getClassType()) {
            case EPrimitive :
                m_primitives.push_back(std::static_pointer_cast<Primitive>(child));
                break;
            case ECamera :
                if (m_camera)
                    throw std::runtime_error("Scene: cannot add multiple cameras!");
                m_camera = std::static_pointer_cast<Camera>(child);
                break;
            default:
                throw std::runtime_error("Scene: cannot add specified child!");
        }
    }

    void Scene::activate() {
        if (!m_camera)
            throw std::runtime_error("Scene: no camera specified!");

        // TODO: checking integrator and sampler availability
        // TODO: BVH activation code
    }

    bool Scene::rayIntersect(const Ray& ray, SurfaceInteraction& isect) const {
        // TODO: reimplement with BVH traversal after implementation

        isect.t = std::numeric_limits<float>::infinity();

        bool hitAny = false;

        for (const auto& primitive : m_primitives) {
            SurfaceInteraction isectLocal;
            if (primitive->rayIntersect(ray, isectLocal)) {
                if (isectLocal.t < isect.t) {
                    isect = isectLocal;
                    hitAny = true;
                }
            }
        }

        return hitAny;
    }

    std::string Scene::toString() const {
        return "Scene[\n  shape = ";
    }

    GND_REGISTER_CLASS(Scene, "scene");
}
