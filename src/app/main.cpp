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
    std::shared_ptr<GndObject> root = Parser::loadFromXML("../scenes/scene.xml");
    std::shared_ptr<Scene> scene = std::static_pointer_cast<Scene>(root);

    int width = scene->getCamera()->getWidth();
    int height = scene->getCamera()->getHeight();

    Bitmap film(width, height);

    std::cout << scene->toString() << std::endl;

    std::cout << "Rendering started..." << std::endl;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            Point2f sample((x + 0.5f) / width, 1.0f - (y + 0.5f) / height);

            Ray ray;
            scene->getCamera()->shootRay(sample, &ray);

            SurfaceInteraction isect;
            Color3f pixelColor(1.0f);

            if (scene->rayIntersect(ray, isect)) {
                pixelColor = Color3f(1.0f, 0.0f, 0.0f) * Dot(isect.n, Vector3f(-1.0f, 1.0f, 1.0f));
            }

            film.setPixel(x, y, pixelColor);
        }
    }

    film.savePNG("scene_render.png");
    std::cout << "Done!" << std::endl;

    return 0;
}