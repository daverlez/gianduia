#pragma once

#include <gianduia/core/propertyList.h>
#include <string>

namespace gnd {

    enum EClassType {
        EScene,
        EPrimitive,
        EShape,
        ECamera,
        EIntegrator,
        ESampler,
        ETexture,
        EMaterial
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
        const std::string& getName() const { return m_name; }

    protected:
        std::string m_name;
    };

    using ObjectFactory = std::function<std::unique_ptr<GndObject>(const PropertyList&)>;

}