#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class InteractiveCamera {
public:
    InteractiveCamera() = default;

    void update(float dx, float dy, float scroll) {
        m_yaw += dx * m_rotationSpeed;
        m_pitch -= dy * m_rotationSpeed;

        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        m_radius -= scroll * m_zoomSpeed;
        if (m_radius < 0.1f) m_radius = 0.1f;
    }

    void setTarget(const glm::vec3& target) { m_target = target; }
    void setRadius(float radius) { m_radius = radius; }
    void setAngles(float pitch, float yaw) {
        m_pitch = pitch;
        m_yaw = yaw;
    }

    glm::mat4 getViewMatrix() const {
        glm::vec3 position;
        position.x = m_target.x + m_radius * cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw));
        position.y = m_target.y + m_radius * sin(glm::radians(m_pitch));
        position.z = m_target.z + m_radius * cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw));

        return glm::lookAt(position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 getProjectionMatrix(float aspect) const {
        return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10000.0f);
    }

private:
    float m_yaw = 0.0f;
    float m_pitch = 20.0f;
    float m_radius = 10.0f;
    glm::vec3 m_target = glm::vec3(0.0f);

    float m_rotationSpeed = 0.3f;
    float m_zoomSpeed = 1.0f;
};