#pragma once
#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>

namespace gnd {

    class Emitter : public GndObject {
    public:
        virtual ~Emitter() = default;

        /// Evaluates emitted radiance at the given surface interaction.
        /// @param isect Surface interaction on the emitter
        /// @param w Outgoing direction at the emitter surface
        virtual Color3f eval(const SurfaceInteraction& isect, const Vector3f& w) const = 0;

        /// Samples a direction from a given shading point. Meant for Next Event Estimation.
        /// @param ref Shading point at which NEE is performed
        /// @param sample 2D uniform sample
        /// @param info [out] Sampled point on the emitter
        /// @param pdf [out] Pdf of the sampled direction with respect to solid angle
        /// @param shadowRay [out] Shadow ray to be traced for occlusion test
        /// @return Emitted radiance along the sampled direction.
        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const = 0;

        /// Computes the pdf of sampling direction wi with respect to solid angle.
        /// @param ref Shading point
        /// @param info Point on the emitter
        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const = 0;

        virtual void setPrimitive(const Primitive* primitive) { m_primitive = primitive; }

        EClassType getClassType() const override { return EEmitter; }

    protected:
        const Primitive* m_primitive = nullptr;
    };

}