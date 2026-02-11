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

        m_bvh = BVH(m_primitives);
        m_bvh.build();
    }

    bool Scene::rayIntersect(const Ray& ray, SurfaceInteraction& isect) const {
        return m_bvh.rayIntersect(ray, isect, false);
    }

    bool Scene::rayIntersect(const Ray& ray) const {
        SurfaceInteraction isect;
        return m_bvh.rayIntersect(ray, isect, true);
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
