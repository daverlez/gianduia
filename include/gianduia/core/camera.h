#pragma once

#include <gianduia/core/object.h>
#include <gianduia/math/transform.h>
#include <gianduia/math/geometry.h>

#include "bitmap.h"

namespace gnd {

    class Camera : public GndObject {
    public:
        Camera(const PropertyList& props) {
            // Default: camera at 0,0,0 looking at -Z.
            m_cameraToWorld = props.getTransform("toWorld", Transform());

            m_outputWidth = props.getInteger("width", 1280);
            m_outputHeight = props.getInteger("height", 720);

            m_film = std::make_unique<Bitmap>(m_outputWidth, m_outputHeight);
        }

        virtual ~Camera() {}

        virtual float shootRay(const Point2f& sample, Ray* ray) const = 0;

        const Transform& getCameraToWorld() const { return m_cameraToWorld; }
        int getWidth() const { return m_outputWidth; }
        int getHeight() const { return m_outputHeight; }
        float getAspectRatio() const { return m_outputWidth / (float)m_outputHeight; }
        Bitmap* getFilm() const { return m_film.get(); }

        EClassType getClassType() const override { return ECamera; }

        void addChild(std::shared_ptr<GndObject> child) override {
            throw std::runtime_error("Camera: addChild operation not supported!");
        }

    protected:
        Transform m_cameraToWorld;
        int m_outputWidth;
        int m_outputHeight;
        std::unique_ptr<Bitmap> m_film;
    };

}
