#include <gianduia/core/camera.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/constants.h>

namespace gnd {

    class PerspectiveCamera : public Camera {
    public:
        explicit PerspectiveCamera(const PropertyList& props) : Camera(props) {
            if (props.hasPoint("origin") || props.hasPoint("target")) {
                Point3f origin = Point3f(props.getPoint("origin", Point3f(0, 0, 0)));
                Point3f target = Point3f(props.getPoint("target", Point3f(0, 0, -1)));
                Vector3f up    = props.getVector("up", Vector3f(0, 1, 0));

                m_cameraToWorld = Transform::LookAt(origin, target, up);
            }

            m_fov = props.getFloat("fov", 45.0f);
            m_scale = std::tan(Radians(m_fov * 0.5f));
        }

        float shootRay(const Point2f& sample, Ray* ray) const override {
            // x: [0, 1] -> [-aspect * scale, +aspect * scale]
            float x = (2.0f * sample.p.x - 1.0f) * getAspectRatio() * m_scale;

            // y: [0, 1] -> [-scale, +scale]
            float y = (2.0f * sample.p.y - 1.0f) * m_scale;

            // Image plane at z = -1.0
            Vector3f dir(x, y, -1.0f);
            dir = Normalize(dir);

            Ray rayCamera(Point3f(0,0,0), dir);
            *ray = m_cameraToWorld(rayCamera);

            return 1.0f;
        }

        std::string toString() const override {
            return "PerspectiveCamera[fov=" + std::to_string(m_fov) + "]";
        }

    private:
        float m_fov;
        float m_scale;
    };

    GND_REGISTER_CLASS(PerspectiveCamera, "perspective");

}