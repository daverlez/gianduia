#include <gianduia/core/camera.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/constants.h>
#include <format>

namespace gnd {

    class PerspectiveCamera : public Camera {
    public:
        explicit PerspectiveCamera(const PropertyList& props) : Camera(props) {
            m_fov = props.getFloat("fov", 45.0f);
            m_scale = std::tan(Radians(m_fov * 0.5f));
        }

        float shootRay(const CameraSample& sample, Ray* ray) const override {
            // x: [0, 1] -> [-aspect * scale, +aspect * scale]
            float x = (2.0f * sample.pFilm.x() - 1.0f) * getAspectRatio() * m_scale;

            // y: [0, 1] -> [-scale, +scale]
            float y = (2.0f * sample.pFilm.y() - 1.0f) * m_scale;

            // Image plane at z = -1.0
            Vector3f dir(x, y, -1.0f);
            dir = Normalize(dir);

            Ray rayCamera(Point3f(0,0,0), dir);
            *ray = m_cameraToWorld(rayCamera);
            ray->time = sample.time;

            return 1.0f;
        }

        std::string toString() const override {
            return std::format(
                "PerspectiveCamera[\n"
                "  width = {}\n"
                "  height = {}\n"
                "  fov = {},\n"
                "  position = {},\n"
                "  forward = {},\n"
                "  up = {}\n"
                "  fiter = {}\n"
                "]",
                m_outputWidth,
                m_outputHeight,
                m_fov,
                m_cameraToWorld.getPosition().toString(),
                m_cameraToWorld.getForward().toString(),
                m_cameraToWorld.getUp().toString(),
                m_film->getFilter()->toString()
            );
        }

    private:
        float m_fov;
        float m_scale;
    };

    GND_REGISTER_CLASS(PerspectiveCamera, "perspective");

}