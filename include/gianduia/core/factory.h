#pragma once

#include <gianduia/core/object.h>
#include <map>
#include <string>
#include <memory>
#include <iostream>

namespace gnd {

    class GndFactory {
    public:
        static GndFactory* getInstance();

        void registerClass(const std::string& name, const ObjectFactory& constructor);

        std::unique_ptr<GndObject> createInstance(const std::string& name, const PropertyList& props);

    private:
        GndFactory() = default;

        // XML name -> pointer to constructor
        std::map<std::string, ObjectFactory> m_constructors;
    };

    #define GND_REGISTER_CLASS(Cls, XmlName) \
        class Cls##_Factory { \
        public: \
            Cls##_Factory() { \
                gnd::GndFactory::getInstance()->registerClass(XmlName, \
                    [](const gnd::PropertyList& props) -> std::unique_ptr<gnd::GndObject> { \
                        return std::make_unique<Cls>(props); \
                    }); \
            } \
        }; \
    static Cls##_Factory global_##Cls##_Factory;

}