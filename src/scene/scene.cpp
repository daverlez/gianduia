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

        if (hitAny)
            isect.primitive->fillInteraction(ray, isect);

        return hitAny;
    }

    bool Scene::rayIntersect(const Ray& ray) const {
        return true;
    }

    std::string Scene::toString() const {
        std::string primitivesStr;
        //
        for (size_t i = 0; i < m_primitives.size(); ++i) {
            primitivesStr += indent(m_primitives[i]->toString());
            if (i + 1 < m_primitives.size())
                primitivesStr += ",\n";
        }

        return std::format(
            "Scene[\n"
            "  camera = \n"
            "{}\n"
            "  primitives = {{\n"
            "{}\n"
            "  }}\n"
            "]",
            indent(m_camera->toString(),2),
            indent(primitivesStr)
        );
    }

    GND_REGISTER_CLASS(Scene, "scene");
}
