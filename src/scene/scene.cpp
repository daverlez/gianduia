#include <gianduia/scene/scene.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/integrator.h>

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
            case EIntegrator :
                if (m_integrator)
                    throw std::runtime_error("Scene: cannot add multiple integrators!");
                m_integrator = std::static_pointer_cast<Integrator>(child);
                break;
            case ESampler :
                if (m_sampler)
                    throw std::runtime_error("Scene: cannot add multiple samplers!");
                m_sampler = std::static_pointer_cast<Sampler>(child);
                break;
            default:
                throw std::runtime_error("Scene: cannot add specified child!");
        }
    }

    void Scene::activate() {
        if (!m_camera)
            throw std::runtime_error("Scene: no camera specified!");
        if (!m_integrator)
            throw std::runtime_error("Scene: no integrator specified!");
        if (!m_sampler)
            throw std::runtime_error("Scene: no sampler specified!");

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

    void Scene::render() {
        m_integrator->render(this);
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
