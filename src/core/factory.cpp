#include <gianduia/core/factory.h>

namespace gnd {

    GndFactory* GndFactory::getInstance() {
        static GndFactory instance;
        return &instance;
    }

    void GndFactory::registerClass(const std::string& name, const ObjectFactory& constructor) {
        if (m_constructors.contains(name)) {
            std::cerr << "Warning: Class '" << name << "' is already defined! Redefining..." << std::endl;
        }
        m_constructors[name] = constructor;
    }

    std::unique_ptr<GndObject> GndFactory::createInstance(const std::string& name, const PropertyList& props) {
        if (!m_constructors.contains(name)) {
            throw std::runtime_error("Cannot instantiate object: class '" + name + "' is not registered!");
        }
        return m_constructors[name](props);
    }

}