#pragma once

#include <gianduia/core/object.h>
#include <map>
#include <string>
#include <iostream>

namespace gnd {

    class GndFactory {
    public:
        static GndFactory* getInstance();

        void registerClass(const std::string& name, const ObjectFactory& constructor);

        GndObject* createInstance(const std::string& name, const PropertyList& props);

    private:
        GndFactory() = default;

        // XML name -> pointer to constructor
        std::map<std::string, ObjectFactory> m_constructors;
    };

}

#define GND_REGISTER_CLASS(Cls, XmlName) \
    gnd::GndObject* Cls##_Constructor(const gnd::PropertyList& list) { \
        return new Cls(list); \
    } \
    static struct Cls##_Registrar { \
        Cls##_Registrar() { \
            gnd::GndFactory::getInstance()->registerClass(XmlName, Cls##_Constructor); \
        } \
    } Cls##_Registrar_Instance;