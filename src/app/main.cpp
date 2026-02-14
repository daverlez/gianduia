#include <iostream>
#include <memory>
#include <limits>

#include "gianduia/core/propertyList.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/object.h"
#include "gianduia/scene/scene.h"
#include "gianduia/core/bitmap.h"
#include "gianduia/core/fileResolver.h"
#include "gianduia/core/parser.h"

using namespace gnd;

int main() {
    std::shared_ptr<GndObject> root = Parser::loadFromXML("../scenes/meshTest.xml");
    std::shared_ptr<Scene> scene = std::static_pointer_cast<Scene>(root);

    std::cout << scene->toString() << std::endl;

    scene->render();

    return 0;
}