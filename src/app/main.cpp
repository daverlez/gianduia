#include "gianduia/core/propertyList.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/camera.h"
#include "gianduia/core/bitmap.h"
#include <iostream>

using namespace gnd;

int main() {
    int width = 800;
    int height = 600;

    PropertyList props;
    props.setInteger("width", width);
    props.setInteger("height", height);
    props.setFloat("fov", 45.0f);

    props.setVector("origin", Vector3f(0, 0, 0));
    props.setVector("target", Vector3f(0, 0, -1));
    props.setVector("up",     Vector3f(0, 1, 0));

    GndObject* obj = GndFactory::getInstance()->createInstance("perspective", props);
    Camera* camera = dynamic_cast<Camera*>(obj);

    Bitmap film(width, height);

    std::cout << "Rendering..." << std::endl;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            Point2f sample( (x + 0.5f) / width, 1.0f - (y + 0.5f) / height );

            Ray ray;
            camera->shootRay(sample, &ray);

            Vector3f d = Normalize(ray.d);

            Color3f color(
                std::abs(d.v.x),
                std::abs(d.v.y),
                std::abs(d.v.z)
            );

            film.setPixel(x, y, color);
        }
    }

    film.savePNG("first_render.png");

    delete camera;

    return 0;
}