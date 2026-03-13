#pragma once

#include <memory>
#include <functional>
#include <string>
#include <format>
#include <gianduia/core/propertyList.h>

namespace gnd {

    enum EClassType {
        EScene,
        EPrimitive,
        EShape,
        ECamera,
        EIntegrator,
        ESampler,
        ETexture,
        EMaterial,
        EEmitter,
        EFilter
    };

    class GndObject {
    public:
        virtual ~GndObject() {}

        virtual void addChild(std::shared_ptr<GndObject> child) {
            throw std::runtime_error(
                "GndObject::addChild() not implemented!"
            );
        }

        virtual void activate() { /* Default: empty */ }

        virtual std::string toString() const = 0;
        virtual EClassType getClassType() const = 0;
        void setName(const std::string& name) { this->m_name = name; }
        const std::string& getName() const { return m_name; }

        static std::string classTypeToString(EClassType type) {
            switch (type) {
                case EScene:      return "scene";
                case EPrimitive:  return "primitive";
                case EShape:       return "shape";
                case ECamera:     return "camera";
                case EIntegrator: return "integrator";
                case ESampler:    return "sampler";
                case ETexture:    return "texture";
                case EMaterial:   return "material";
                case EEmitter:    return "emitter";
                case EFilter:     return "filter";
                default:          return "unknown";
            }
        }

    protected:
        std::string m_name;
    };

    using ObjectFactory = std::function<std::unique_ptr<GndObject>(const PropertyList&)>;

}