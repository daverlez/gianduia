#pragma once
#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>

namespace gnd {

    enum EmitterType {
        EMITTER_POINT       = 1 << 0,
        EMITTER_DIRECTIONAL = 1 << 1,
        EMITTER_AREA        = 1 << 2,
        EMITTER_ENV         = 1 << 3,

        EMITTER_DELTA       = EMITTER_POINT | EMITTER_DIRECTIONAL
    };

    class Emitter : public GndObject {
    public:
        explicit Emitter(EmitterType type = EMITTER_AREA) : m_type(type) {}
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
        /// @return Emitted radiance along the sampled direction, divided by the pdf.
        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const = 0;

        /// Computes the pdf of sampling direction wi with respect to solid angle.
        /// @param ref Shading point
        /// @param info Point on the emitter
        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const = 0;

        /// Samples a photon from the light source.
        /// @param uPos 2D uniform sample to pick a position on the emitter.
        /// @param uDir 2D uniform sample to pick an emission direction.
        /// @param time Emission time (for motion blur)
        /// @param photonRay [out] Ray representing the initial trajectory of the photon.
        /// @return Power associated with the photon, divided by the pdf (positional and directional).
        virtual Color3f samplePhoton(const Point2f& uPos, const Point2f& uDir, float time, Ray& photonRay) const = 0;

        virtual void setPrimitive(const Primitive* primitive) { m_primitive = primitive; }

        bool isDelta() const { return (m_type & EMITTER_DELTA) != 0; }
        virtual bool isInfiniteAreaLight() const { return false; }
        EmitterType getType() const { return m_type; }

        EClassType getClassType() const override { return EEmitter; }

    protected:
        const Primitive* m_primitive = nullptr;
        EmitterType m_type;
    };

}