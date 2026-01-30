#pragma once

#include <string>
#include <memory>
#include <gianduia/core/object.h>

namespace gnd {

    class Parser {
    public:
        /// Loads the scene from an XML file.
        /// @param filename Path to the XML file.
        /// @return Unique points to the root of the scene.
        static std::unique_ptr<GndObject> loadFromXML(const std::string& filename);
    };

}