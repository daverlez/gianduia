#pragma once
#include <glad/glad.h>
#include <vector>

#include "app/interactiveCamera.h"

#include "gianduia/core/bvhBuilder.h"

class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer();

    void init();
    void resize(int width, int height);

    void buildBuffers(const std::vector<gnd::BvhDebugNode>& nodes, int maxTlasDepth, int maxBlasDepth);

    void render(const InteractiveCamera& camera) const;
    
    GLuint getOutputTexture() const { return m_outputTexture; }

private:
    static void checkCompileErrors(GLuint shader, std::string type);

    bool m_isInitialized = false;

    GLuint m_shaderProgram = 0;
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;

    GLuint m_FBO = 0;
    GLuint m_outputTexture = 0;
    GLuint m_depthRenderbuffer = 0;

    int m_width = 0;
    int m_height = 0;
    int m_vertexCount = 0;
};
