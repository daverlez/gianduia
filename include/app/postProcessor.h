#pragma once
#include <glad/glad.h>
#include <iostream>
#include <vector>

#include "app/glShader.h"

enum class TonemapOperator {
    Linear = 0,
    ACES = 1,
    KhronosPBR = 2
};

class PostProcessor {
public:
    PostProcessor() = default;
    ~PostProcessor();

    void init();
    void resize(int width, int height);

    void render(GLuint inputTextureID, TonemapOperator tonemapper);
    GLuint getOutputTexture() const { return m_displayTexture; }
    std::vector<float> getTonemappedData() const;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    bool m_isInitialized = false;

    glShader m_tonemapShader;
    glShader m_gammaCorrectionShader;

    GLuint m_VAO = 0;
    GLuint m_VBO = 0;

    // Linear FBOs
    GLuint m_hdrFBO = 0;
    GLuint m_hdrTexture = 0;

    // Displayed, gamma corrected FBO
    GLuint m_ldrFBO = 0;
    GLuint m_displayTexture = 0;

    int m_width = 0;
    int m_height = 0;
};