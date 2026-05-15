#include <app/postProcessor.h>

PostProcessor::~PostProcessor() {
    if (!m_isInitialized) return;

    glDeleteFramebuffers(1, &m_hdrFBO);
    glDeleteTextures(1, &m_hdrTexture);

    glDeleteFramebuffers(1, &m_ldrFBO);
    glDeleteTextures(1, &m_displayTexture);

    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
}

void PostProcessor::init() {
    if (m_isInitialized) return;

    m_tonemapShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/tonemap.frag");

    m_gammaCorrectionShader = glShader(
        "assets/shaders/fullscreen.vert",
        "assets/shaders/gammaCorrect.frag");

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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::render(GLuint inputTextureID, TonemapOperator tonemapper) {
    glViewport(0, 0, m_width, m_height);

    // Pass 1: Tonemapping
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    m_tonemapShader.use();
    m_tonemapShader.setInt("screenTexture", 0);
    m_tonemapShader.setInt("u_tonemapper", static_cast<int>(tonemapper));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTextureID);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Pass 2: Gamma Correction
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
