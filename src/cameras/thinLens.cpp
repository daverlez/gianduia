#include <gianduia/core/camera.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/constants.h>
#include <cmath>
#include <format>

#include "gianduia/math/warp.h"

namespace gnd {

    class ThinLensCamera : public Camera {
    public:
        explicit ThinLensCamera(const PropertyList& props) : Camera(props) {
            m_fov = props.getFloat("fov", 45.0f);
            m_scale = std::tan(Radians(m_fov * 0.5f));

            m_lensRadius = props.getFloat("lens_radius", 0.0f);
            m_focalDistance = props.getFloat("focal_distance", 10.0f);

            m_k1 = props.getFloat("k1", 0.0f);

            m_caAxial = props.getFloat("ca_axial", 0.0f);
            m_caLateral = props.getFloat("ca_lateral", 0.0f);

            m_vignetting = props.getBoolean("vignetting", false);

            m_blades = props.getInteger("blades", 0);

            m_rollingShutter = props.getBoolean("rolling_shutter", false);
            m_scanDuration = props.getFloat("scan_duration", 0.9f);
            m_exposureDuration = props.getFloat("exposure_duration", 0.1f);
        }

        float shootRay(const CameraSample& sample, Ray* ray) const override {
            float ndcX = 2.0f * sample.pFilm.x() - 1.0f;
            float ndcY = 2.0f * sample.pFilm.y() - 1.0f;

            // Barrel Distortion
            float r2 = ndcX * ndcX + ndcY * ndcY;
            float distortion = 1.0f + m_k1 * r2;
            float distX = ndcX * distortion;
            float distY = ndcY * distortion;

            // Lateral CA
            float scaleMod = m_scale * (1.0f + m_caLateral * sample.lambdaOffset);
            
            float x = distX * getAspectRatio() * scaleMod;
            float y = distY * scaleMod;

            Vector3f dir(x, y, -1.0f);
            Point3f rayOrigin(0.0f, 0.0f, 0.0f);

            // Depth of Field & Axial CA
            if (m_lensRadius > 0.0f) {
                Point2f pLens;
                if (m_blades >= 3) {
                    pLens = Warp::squareToUniformPolygon(sample.pLens, m_blades) * m_lensRadius;
                } else {
                    pLens = Warp::squareToUniformDisk(sample.pLens) * m_lensRadius;
                }

                float focalDistMod = m_focalDistance * (1.0f + m_caAxial * sample.lambdaOffset);

                float ft = focalDistMod / std::abs(dir.z());
                Point3f pFocus = rayOrigin + dir * ft;

                rayOrigin = Point3f(pLens.x(), pLens.y(), 0.0f);
                dir = pFocus - rayOrigin;
            }

            dir = Normalize(dir);

            // Vignetting
            float weight = 1.0f;
            if (m_vignetting) {
                float cosTheta = std::abs(dir.z());
                float cos2 = cosTheta * cosTheta;
                weight = cos2 * cos2;
            }

            // Rolling shutter
            float finalTime = sample.time; // Fallback to standard motion blur
            if (m_rollingShutter) {
                float scanline = 1.0f - sample.pFilm.y();

                finalTime = scanline * m_scanDuration + (sample.time * m_exposureDuration);
            }

            Ray rayCamera(rayOrigin, dir);
            Transform toWorld = m_cameraToWorld.interpolate(finalTime);
            *ray = toWorld(rayCamera);
            ray->time = finalTime;

            return weight;
        }

        bool hasChromaticAberration() const override {
            return m_caLateral != 0.0f || m_caAxial != 0.0f;
        }

        void getProjectionMatrix(float zNear, float zFar, float* outMatrix16) const override {
            float aspect = getAspectRatio();
            Transform proj = Transform::Perspective(m_fov, aspect, zNear, zFar);
            proj.getMatrixData(outMatrix16);
        }

        std::string toString() const override {
            return std::format(
                "ThinLensCamera[\n"
                "  width = {}\n"
                "  height = {}\n"
                "  fov = {},\n"
                "  position = {},\n"
                "  forward = {},\n"
                "  up = {}\n"
                "  fiter = {}\n"
                "  k1 = {}\n"
                "  lens radius = {}\n"
                "  focal distance = {}\n"
                "  chromatic aberration (lateral) = {}\n"
                "  chromatic aberration (axial) = {}\n"
                "  vignetting = {}\n"
                "  rolling shutter = {}\n"
                "]",
                m_outputWidth,
                m_outputHeight,
                m_fov,
                m_cameraToWorld.interpolate(0.0f).getPosition().toString(),
                m_cameraToWorld.interpolate(0.0f).getForward().toString(),
                m_cameraToWorld.interpolate(0.0f).getUp().toString(),
                m_film->getFilter()->toString(),
                m_k1,
                m_lensRadius,
                m_focalDistance,
                m_caLateral,
                m_caAxial,
                m_vignetting ? "true" : "false",
                m_rollingShutter ? "true" : "false"
            );
        }

    private:
        float m_fov;
        float m_scale;

        float m_lensRadius;
        float m_focalDistance;

        float m_k1;

        float m_caAxial;
        float m_caLateral;

        bool m_vignetting;

        int m_blades;

        bool m_rollingShutter;
        float m_scanDuration;
        float m_exposureDuration;
    };

    GND_REGISTER_CLASS(ThinLensCamera, "thinlens");
}
