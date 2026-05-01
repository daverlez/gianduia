#pragma once

#include <gianduia/core/object.h>
#include <gianduia/math/transform.h>
#include <gianduia/math/geometry.h>
#include <gianduia/math/animated.h>
#include <gianduia/core/film.h>

namespace gnd {

    struct CameraSample {
        Point2f pFilm;
        Point2f pLens;      // Depth of Field sample
        float lambdaOffset; // [-1, 0, 1] for chromatic aberration
        float time;
    };

    class Camera : public GndObject {
    public:
        Camera(const PropertyList& props) {
            // Default: camera at 0,0,0 looking at -Z.
            Transform tStart = props.getTransform("toWorld", Transform());
            Transform tEnd = props.getTransform("toWorldEnd", tStart);

            m_cameraToWorld = AnimatedTransform(tStart, 0.0f, tEnd, 1.0f);

            m_outputWidth = props.getInteger("width", 1280);
            m_outputHeight = props.getInteger("height", 720);
        }
        virtual ~Camera() {}

        virtual float shootRay(const CameraSample& sample, Ray* ray) const = 0;
        virtual bool hasChromaticAberration() const { return false; }

        Transform getCameraToWorld(float time) const { return m_cameraToWorld.interpolate(time); }
        int getWidth() const { return m_outputWidth; }
        int getHeight() const { return m_outputHeight; }
        float getAspectRatio() const { return m_outputWidth / (float)m_outputHeight; }
        Film* getFilm() const { return m_film.get(); }
        Vector3f getForward() const {
            Transform toWorld = m_cameraToWorld.interpolate(0.0f);
            return toWorld(Vector3f(0, 0, -1.0f));
        }

        virtual void getViewMatrix(float time, float* outMatrix16) const {
            Transform worldToCam = getCameraToWorld(time).inverse();
            worldToCam.getMatrixData(outMatrix16);
        }

        virtual void getProjectionMatrix(float zNear, float zFar, float* outMatrix16) const = 0;

        EClassType getClassType() const override { return ECamera; }

        virtual void addChild(std::shared_ptr<GndObject> child) {
            if (child->getClassType() == EFilter) {
                if (m_film)
                    throw std::runtime_error("Camera: cannot define multiple films!");
                m_film = std::make_unique<Film>(m_outputWidth, m_outputHeight,
                            std::static_pointer_cast<Filter>(child));
            }
        }

        void activate() {
            if (!m_film)
                m_film = std::make_unique<Film>(m_outputWidth, m_outputHeight);
        }

    protected:
        AnimatedTransform m_cameraToWorld;
        int m_outputWidth;
        int m_outputHeight;
        std::unique_ptr<Film> m_film;
    };

}
