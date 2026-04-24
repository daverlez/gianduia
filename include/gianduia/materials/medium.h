#pragma once

#include <gianduia/core/object.h>
#include <gianduia/core/sampler.h>
#include <gianduia/core/arena.h>
#include <gianduia/math/ray.h>
#include <gianduia/math/phase.h>

namespace gnd {

    class Medium;

    struct MediumInteraction {
        Point3f p;
        Vector3f wo;
        float time;
        const Medium* medium = nullptr;

        const PhaseFunction* phase = nullptr;

        bool isValid() const { return medium != nullptr; }
    };

    class Medium : public GndObject {
    public:
        virtual ~Medium() = default;

        virtual Color3f Tr(const Ray& ray, Sampler& sampler) const = 0;

        virtual Color3f Le(const Point3f& p) const {
            return Color3f(0.0f);
        }

        virtual Color3f sample(const Ray& ray, Sampler& sampler, MemoryArena& arena, MediumInteraction& mi) const = 0;

        virtual float getDensity(const Point3f& p) const = 0;

        EClassType getClassType() const override { return EMedium; }
    };
}
