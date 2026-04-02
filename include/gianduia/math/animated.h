#pragma once
#include <gianduia/math/transform.h>
#include <gianduia/math/bounds.h>
#include <glm/gtc/quaternion.hpp>

namespace gnd {

    class AnimatedTransform {
    public:
        AnimatedTransform();
        AnimatedTransform(const Transform& t1, float time1, const Transform& t2, float time2);
        explicit AnimatedTransform(const Transform& t);

        Transform interpolate(float time) const;
        Bounds3f motionBounds(const Bounds3f& b) const;

        bool isAnimated() const { return m_actuallyAnimated; }

        const Transform& getStartTransform() const { return m_startTransform; }
        const Transform& getEndTransform() const { return m_endTransform; }

    private:
        Transform m_startTransform;
        Transform m_endTransform;
        float m_startTime;
        float m_endTime;
        bool m_actuallyAnimated;

        glm::vec3 m_T[2];
        glm::quat m_R[2];
        glm::vec3 m_S[2];

        void decompose(const glm::mat4& m, glm::vec3* T, glm::quat* R, glm::vec3* S);
    };

}