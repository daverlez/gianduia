#include "app/debugRenderer.h"
#include <iostream>

struct DebugVertex {
    glm::vec3 position;
    glm::vec3 color;
};

DebugRenderer::~DebugRenderer() {
    if (!m_isInitialized) return;

    glDeleteFramebuffers(1, &m_FBO);
    glDeleteTextures(1, &m_outputTexture);
    glDeleteRenderbuffers(1, &m_depthRenderbuffer);
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteProgram(m_shaderProgram);
}

void DebugRenderer::init() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;

        out vec3 vertexColor;

        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            vertexColor = aColor;
            gl_Position = projection * view * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        in vec3 vertexColor;

        void main() {
            FragColor = vec4(vertexColor, 1.0);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkCompileErrors(vertexShader, "VERTEX");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT");

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);
    checkCompileErrors(m_shaderProgram, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, color));

    glBindVertexArray(0);

    m_isInitialized = true;
}

void DebugRenderer::resize(int width, int height) {
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;

    if (m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
        glDeleteTextures(1, &m_outputTexture);
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
    }

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    glGenTextures(1, &m_outputTexture);
    glBindTexture(GL_TEXTURE_2D, m_outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outputTexture, 0);

    glGenRenderbuffers(1, &m_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::DEBUG_RENDERER:: Framebuffer is not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DebugRenderer::buildBuffers(const std::vector<gnd::BvhDebugNode>& nodes, int maxTlasDepth, int maxBlasDepth) {
    if (!m_isInitialized) return;

    std::vector<DebugVertex> vertices;
    vertices.reserve(nodes.size() * 24);

    for (const auto& node : nodes) {
        glm::vec3 color;

        if (!node.isBlas) {
            float t = (maxTlasDepth > 0) ? (float)node.depth / maxTlasDepth : 0.0f;
            color = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), t);
        } else {
            float t = (maxBlasDepth > 0) ? (float)node.depth / maxBlasDepth : 0.0f;
            color = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.5f, 1.0f), t);
        }

        glm::vec3 minBound(node.bounds.pMin.x(), node.bounds.pMin.y(), node.bounds.pMin.z());
        glm::vec3 maxBound(node.bounds.pMax.x(), node.bounds.pMax.y(), node.bounds.pMax.z());

        glm::vec3 size = maxBound - minBound;
        float margin = 0.005f * node.depth;

        minBound += size * margin;
        maxBound -= size * margin;

        glm::vec3 corners[8] = {
            glm::vec3(minBound.x, minBound.y, minBound.z), glm::vec3(maxBound.x, minBound.y, minBound.z),
            glm::vec3(maxBound.x, maxBound.y, minBound.z), glm::vec3(minBound.x, maxBound.y, minBound.z),
            glm::vec3(minBound.x, minBound.y, maxBound.z), glm::vec3(maxBound.x, minBound.y, maxBound.z),
            glm::vec3(maxBound.x, maxBound.y, maxBound.z), glm::vec3(minBound.x, maxBound.y, maxBound.z)
        };

        int lineIndices[24] = {
            0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7
        };

        for (int i = 0; i < 24; ++i) {
            vertices.push_back({ corners[lineIndices[i]], color });
        }
    }

    m_vertexCount = vertices.size();

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(DebugVertex), vertices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void DebugRenderer::render(const InteractiveCamera& camera) const {
    if (!m_isInitialized) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_width, m_height);

    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_vertexCount > 0) {
        glUseProgram(m_shaderProgram);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix((float)m_width / (float)m_height);

        glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);

        glBindVertexArray(m_VAO);
        glDisable(GL_DEPTH_TEST);

        glDrawArrays(GL_LINES, 0, m_vertexCount);

        glBindVertexArray(0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DebugRenderer::checkCompileErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}