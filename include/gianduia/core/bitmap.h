#pragma once

#include <gianduia/math/color.h>
#include <vector>
#include <string>

namespace gnd {

    class Bitmap {
    public:
        Bitmap(int width, int height)
            : m_width(width), m_height(height) {
            m_pixels.resize(width * height);
        }

        void setPixel(int x, int y, const Color3f& color) {
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;

            m_pixels[y * m_width + x] = color;
        }

        const Color3f& getPixel(int x, int y) const {
            return m_pixels[y * m_width + x];
        }

        void clear() {
            std::ranges::fill(m_pixels, Color3f(0.0f));
        }

        void savePNG(const std::string& filename) const;

        int width() const { return m_width; }
        int height() const { return m_height; }
        float aspectRatio() const { return (float)m_width / (float)m_height; }

    private:
        int m_width, m_height;
        std::vector<Color3f> m_pixels;
    };

}