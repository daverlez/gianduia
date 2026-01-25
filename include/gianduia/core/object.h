#pragma once

#include <gianduia/core/propertyList.h>
#include <string>

namespace gnd {

    class GndObject {
    public:
        virtual ~GndObject() {}

        virtual void addChild(GndObject* child) {
            throw std::runtime_error(
                "GndObject::addChild() not implemented for class " + getClassType()
            );
        }

        virtual void activate() { /* Default: empty */ }

        virtual std::string toString() const = 0;
        virtual std::string getClassType() const = 0;
    };

    using ObjectFactory = GndObject* (*)(const PropertyList&);

}