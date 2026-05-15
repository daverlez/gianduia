#pragma once
#include <glad/glad.h>
#include <iostream>
#include <vector>

#include "app/glShader.h"

class PostProcessor {
public:
    PostProcessor() = default;
    ~PostProcessor();

    void init();
    void resize(int width, int height);
    void render(GLuint inputTextureID);
    GLuint getOutputTexture() const { return m_outputTexture; }

private:
    bool m_isInitialized = false;

    glShader m_gammaCorrectionShader;

    GLuint m_VAO = 0;
    GLuint m_VBO = 0;

    GLuint m_FBO = 0;
    GLuint m_outputTexture = 0;

    int m_width = 0;
    int m_height = 0;
};
