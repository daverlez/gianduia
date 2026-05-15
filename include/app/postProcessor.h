#pragma once
#include <glad/glad.h>
#include <iostream>
#include <vector>

#include "app/glShader.h"

enum class TonemapOperator {
    Linear = 0,
    Reinhard = 1,
    Uncharted2 = 2,
    ACES = 3,
    KhronosPBR = 4,
    AgX = 5,
    AgXPunchy = 6
};

struct BloomMip {
    int width;
    int height;
    GLuint texture;
    GLuint fbo;
};

class PostProcessor {
public:
    PostProcessor() = default;
    ~PostProcessor();

    TonemapOperator tonemapper = TonemapOperator::Linear;
    float exposure = 0.0f;

    bool enableBloom = false;
    float bloomIntensity = 0.04f;
    float bloomThreshold = 1.0f;
    float bloomRadius = 1.0f;
    int activeBloomMips = 6;

    void init();
    void resize(int width, int height);

    void render(GLuint inputTextureID);
    GLuint getOutputTexture() const { return m_displayTexture; }
    std::vector<float> getTonemappedData() const;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getBloomMipsCount() const { return static_cast<int>(m_bloomMips.size()); }

private:
    bool m_isInitialized = false;

    glShader m_tonemapShader;
    glShader m_gammaCorrectionShader;

    glShader m_bloomPrefilterShader;
    glShader m_bloomDownsampleShader;
    glShader m_bloomUpsampleShader;

    std::vector<BloomMip> m_bloomMips;

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