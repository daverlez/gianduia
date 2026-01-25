#include <gianduia/core/bitmap.h>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace gnd {

    void Bitmap::savePNG(const std::string& filename) const {
        std::vector<uint8_t> data(m_width * m_height * 3);

        int index = 0;
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                Color3f linearColor = m_pixels[y * m_width + x];
                Color3f srgbColor = linearColor.toSRGB();

                Color3f final = srgbColor * 255.0f + Color3f(0.5);
                final = final.clamp(0.0f, 255.0f);

                data[index++] = static_cast<uint8_t>(final.r());
                data[index++] = static_cast<uint8_t>(final.g());
                data[index++] = static_cast<uint8_t>(final.b());
            }
        }

        int success = stbi_write_png(filename.c_str(), m_width, m_height, 3, data.data(), m_width * 3);

        if (!success) {
            std::cerr << "Error while saving image: " << filename << std::endl;
        } else {
            std::cout << "PNG image saved successfully: " << filename << std::endl;
        }
    }

}