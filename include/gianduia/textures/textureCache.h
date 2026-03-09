#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include <stb_image.h>

#include <gianduia/math/color.h>

namespace gnd {

    struct ImageData {
        int width = 0;
        int height = 0;
        std::vector<Color3f> pixels;
    };

    class TextureCache {
    public:
        static std::shared_ptr<ImageData> load(const std::string& filepath, bool isSRGB) {
            std::string key = filepath + (isSRGB ? "_srgb" : "_linear");

            if (m_cache.find(key) != m_cache.end()) {
                return m_cache[key];
            }

            auto data = std::make_shared<ImageData>();
            int originalChannels;

            stbi_set_flip_vertically_on_load(true);
            unsigned char* rawData = stbi_load(filepath.c_str(), &data->width, &data->height, &originalChannels, 3);

            if (!rawData) {
                throw std::runtime_error("TextureCache: Failed to load image " + filepath);
            }

            int totalPixels = data->width * data->height;
            data->pixels.resize(totalPixels);

            for (int i = 0; i < totalPixels; ++i) {
                float r = rawData[i * 3 + 0] / 255.0f;
                float g = rawData[i * 3 + 1] / 255.0f;
                float b = rawData[i * 3 + 2] / 255.0f;

                Color3f c(r, g, b);
                if (isSRGB) {
                    c = c.toLinear();
                }
                data->pixels[i] = c;
            }

            stbi_image_free(rawData);
            m_cache[key] = data;

            return data;
        }

        static void clear() {
            m_cache.clear();
        }

    private:
        inline static std::unordered_map<std::string, std::shared_ptr<ImageData>> m_cache;
    };

}