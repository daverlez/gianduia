#include <app/postProcessor.h>

#include <algorithm>

PostProcessor::~PostProcessor() {
    if (!m_isInitialized) return;

    glDeleteFramebuffers(1, &m_hdrFBO);
    glDeleteTextures(1, &m_hdrTexture);

    glDeleteFramebuffers(1, &m_ldrFBO);
    glDeleteTextures(1, &m_displayTexture);

    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);

    for (const auto& mip : m_bloomMips) {
        glDeleteFramebuffers(1, &mip.fbo);
        glDeleteTextures(1, &mip.texture);
    }
}

void PostProcessor::init() {
    if (m_isInitialized) return;

    m_tonemapShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/tonemap.frag");
    m_gammaCorrectionShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/gammaCorrect.frag");
    m_bloomPrefilterShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/bloom_prefilter.frag");
    m_bloomDownsampleShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/bloom_downsample.frag");
    m_bloomUpsampleShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/bloom_upsample.frag");

    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    m_isInitialized = true;
}

void PostProcessor::resize(int width, int height) {
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;

    if (m_hdrFBO) {
        glDeleteFramebuffers(1, &m_hdrFBO);
        glDeleteTextures(1, &m_hdrTexture);
    }

    if (m_ldrFBO) {
        glDeleteFramebuffers(1, &m_ldrFBO);
        glDeleteTextures(1, &m_displayTexture);
    }

    for (const auto& mip : m_bloomMips) {
        glDeleteFramebuffers(1, &mip.fbo);
        glDeleteTextures(1, &mip.texture);
    }
    m_bloomMips.clear();

    // FBO 1: Tonemapping (Float)
    glGenFramebuffers(1, &m_hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);

    glGenTextures(1, &m_hdrTexture);
    glBindTexture(GL_TEXTURE_2D, m_hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrTexture, 0);

    // FBO 2: Gamma Correction (LDR / 8-bit)
    glGenFramebuffers(1, &m_ldrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ldrFBO);

    glGenTextures(1, &m_displayTexture);
    glBindTexture(GL_TEXTURE_2D, m_displayTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_displayTexture, 0);

    for (const auto& mip : m_bloomMips) {
        glDeleteFramebuffers(1, &mip.fbo);
        glDeleteTextures(1, &mip.texture);
    }
    m_bloomMips.clear();

    // Bloom pyramid (five levels)
    int mipWidth = width / 2;
    int mipHeight = height / 2;
    const int maxMips = 6;

    for (int i = 0; i < maxMips; i++) {
        BloomMip mip;
        mip.width = mipWidth;
        mip.height = mipHeight;

        glGenFramebuffers(1, &mip.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);

        glGenTextures(1, &mip.texture);
        glBindTexture(GL_TEXTURE_2D, mip.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, mipWidth, mipHeight, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);
        m_bloomMips.push_back(mip);

        mipWidth /= 2;
        mipHeight /= 2;
        if (mipWidth < 2 || mipHeight < 2) break;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::render(GLuint inputTextureID) {
    glViewport(0, 0, m_width, m_height);
    glBindVertexArray(m_VAO);

    // Bloom pass
    if (enableBloom && !m_bloomMips.empty()) {
        int renderMips = std::min(activeBloomMips, static_cast<int>(m_bloomMips.size()));

        // Prefilter
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomMips[0].fbo);
        glViewport(0, 0, m_bloomMips[0].width, m_bloomMips[0].height);

        m_bloomPrefilterShader.use();
        m_bloomPrefilterShader.setInt("srcTexture", 0);
        m_bloomPrefilterShader.setFloat("u_threshold", bloomThreshold);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTextureID);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Downsample
        m_bloomDownsampleShader.use();
        m_bloomDownsampleShader.setInt("srcTexture", 0);

        for (size_t i = 1; i < renderMips; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloomMips[i].fbo);
            glViewport(0, 0, m_bloomMips[i].width, m_bloomMips[i].height);

            m_bloomDownsampleShader.setVec2("u_srcResolution", glm::vec2(m_bloomMips[i-1].width, m_bloomMips[i-1].height));

            glBindTexture(GL_TEXTURE_2D, m_bloomMips[i-1].texture);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // Upsample
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        m_bloomUpsampleShader.use();
        m_bloomUpsampleShader.setInt("srcTexture", 0);
        m_bloomUpsampleShader.setFloat("u_filterRadius", bloomRadius);

        for (int i = renderMips - 1; i > 0; i--) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloomMips[i-1].fbo);
            glViewport(0, 0, m_bloomMips[i-1].width, m_bloomMips[i-1].height);

            glBindTexture(GL_TEXTURE_2D, m_bloomMips[i].texture);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glDisable(GL_BLEND);
    }

    // Tonemapping pass
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);

    m_tonemapShader.use();
    m_tonemapShader.setInt("screenTexture", 0);
    m_tonemapShader.setInt("u_tonemapper", static_cast<int>(tonemapper));
    m_tonemapShader.setFloat("u_exposure", exposure);
    m_tonemapShader.setInt("bloomTexture", 1);
    m_tonemapShader.setBool("u_enableBloom", enableBloom);
    m_tonemapShader.setFloat("u_bloomIntensity", bloomIntensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTextureID);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomMips.empty() ? 0 : m_bloomMips[0].texture);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Gamma Correction pass
    glBindFramebuffer(GL_FRAMEBUFFER, m_ldrFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    m_gammaCorrectionShader.use();
    m_gammaCorrectionShader.setInt("screenTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrTexture);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::vector<float> PostProcessor::getTonemappedData() const {
    std::vector<float> data(m_width * m_height * 3);

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glReadPixels(0, 0, m_width, m_height, GL_RGB, GL_FLOAT, data.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return data;
}
