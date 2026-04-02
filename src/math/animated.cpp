#include <gianduia/math/animated.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace gnd {

    AnimatedTransform::AnimatedTransform() 
        : m_startTime(0.0f), m_endTime(1.0f), m_actuallyAnimated(false) {}

    AnimatedTransform::AnimatedTransform(const Transform& t)
        : m_startTransform(t), m_endTransform(t), m_startTime(0.0f), m_endTime(1.0f), m_actuallyAnimated(false) {}

    AnimatedTransform::AnimatedTransform(const Transform& t1, float time1, const Transform& t2, float time2)
        : m_startTransform(t1), m_endTransform(t2), m_startTime(time1), m_endTime(time2) {
        
        m_actuallyAnimated = (t1 != t2);

        if (m_actuallyAnimated) {
            decompose(t1.getMatrix(), &m_T[0], &m_R[0], &m_S[0]);
            decompose(t2.getMatrix(), &m_T[1], &m_R[1], &m_S[1]);
        }
    }

    void AnimatedTransform::decompose(const glm::mat4& m, glm::vec3* T, glm::quat* R, glm::vec3* S) {
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(m, *S, *R, *T, skew, perspective);
    }

    Transform AnimatedTransform::interpolate(float time) const {
        if (!m_actuallyAnimated || time <= m_startTime) return m_startTransform;
        if (time >= m_endTime) return m_endTransform;

        float dt = (time - m_startTime) / (m_endTime - m_startTime);

        glm::vec3 trans = glm::mix(m_T[0], m_T[1], dt);
        glm::vec3 scale = glm::mix(m_S[0], m_S[1], dt);
        glm::quat rot = glm::slerp(m_R[0], m_R[1], dt);

        glm::mat4 T_mat = glm::translate(glm::mat4(1.0f), trans);
        glm::mat4 R_mat = glm::mat4_cast(rot);
        glm::mat4 S_mat = glm::scale(glm::mat4(1.0f), scale);

        glm::mat4 M = T_mat * R_mat * S_mat;
        
        return Transform(M);
    }

    Bounds3f AnimatedTransform::motionBounds(const Bounds3f& b) const {
        if (!m_actuallyAnimated) return m_startTransform(b);

        Bounds3f ret = m_startTransform(b);
        ret = Union(ret, m_endTransform(b));

        Transform tMid = interpolate((m_startTime + m_endTime) * 0.5f);
        ret = Union(ret, tMid(b));

        return ret;
    }

}