#include <iostream>
#include <memory>
#include <limits>

#include "gianduia/core/propertyList.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/object.h"
#include "gianduia/scene/scene.h"
#include "gianduia/core/bitmap.h"

using namespace gnd;

int main() {
    PropertyList camProps;
    camProps.setInteger("width", 800);
    camProps.setInteger("height", 600);
    camProps.setFloat("fov", 45.0f);
    camProps.setVector("origin", Vector3f(0, 0, 0));
    camProps.setVector("target", Vector3f(0, 0, -1));
    camProps.setVector("up", Vector3f(0, 1, 0));

    std::shared_ptr<GndObject> camera = GndFactory::getInstance()->createInstance("perspective", camProps);
    camera->activate();

    PropertyList sphereProps;
    sphereProps.setFloat("radius", 1.0f);

    std::shared_ptr<GndObject> sphere = GndFactory::getInstance()->createInstance("sphere", sphereProps);
    sphere->activate();

    PropertyList primProps;
    primProps.setTransform("toWorld", Transform::Translate(Vector3f(0, 0, -4)));

    std::shared_ptr<GndObject> primitive = GndFactory::getInstance()->createInstance("primitive", primProps);
    primitive->addChild(sphere);
    primitive->activate();

    PropertyList sceneProps;
    std::shared_ptr<GndObject> sceneObj = GndFactory::getInstance()->createInstance("scene", sceneProps);

    sceneObj->addChild(camera);
    sceneObj->addChild(primitive);
    sceneObj->activate();

    auto scene = std::dynamic_pointer_cast<Scene>(sceneObj);
    if (!scene) return -1;

    auto renderCamera = scene->getCamera();
    if (!renderCamera) return -1;

    int width = camProps.getInteger("width");
    int height = camProps.getInteger("height");

    Bitmap film(width, height);

    std::cout << "Rendering started..." << std::endl;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            Point2f sample((x + 0.5f) / width, 1.0f - (y + 0.5f) / height);

            Ray ray;
            renderCamera->shootRay(sample, &ray);

            SurfaceInteraction isect;
            Color3f pixelColor(0.0f);

            if (scene->rayIntersect(ray, isect)) {
                pixelColor = Color3f((isect.n[0] + 1.0f) * 0.5f,
                                     (isect.n[1] + 1.0f) * 0.5f,
                                     (isect.n[2] + 1.0f) * 0.5f);
            }

            film.setPixel(x, y, pixelColor);
        }
    }

    film.savePNG("scene_render.png");
    std::cout << "Done!" << std::endl;

    return 0;
}