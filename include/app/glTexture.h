#pragma once
#include <glad/glad.h>
#include <gianduia/core/bitmap.h>

class glTexture {
public:
    GLuint id = 0;
    int width = 0, height = 0;

    ~glTexture() {
        if (id) glDeleteTextures(1, &id);
    }

    void update(const gnd::Bitmap& bitmap) {
        if (id == 0 || width != bitmap.width() || height != bitmap.height()) {
            width = bitmap.width();
            height = bitmap.height();
            if (id) glDeleteTextures(1, &id);
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glBindTexture(GL_TEXTURE_2D, id);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, bitmap.data());
    }
};