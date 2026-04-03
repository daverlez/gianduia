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

    void update(gnd::Bitmap& bitmap) {
        update(bitmap.width(), bitmap.height(), bitmap.data());
    }

    void update(int _width, int _height, const float* data) {
        if (id == 0 || width != _width || height != _height) {
            width = _width;
            height = _height;
            if (id) glDeleteTextures(1, &id);
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glBindTexture(GL_TEXTURE_2D, id);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);
    }
};