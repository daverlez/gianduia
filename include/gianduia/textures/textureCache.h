#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <stdexcept>
#include <stb_image.h>
#include <gianduia/math/color.h>

// Header OpenEXR
#include <ImfRgbaFile.h>
#include <ImfArray.h>

namespace gnd {

    struct ImageData {
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<float> data;
    };

    class TextureCache {
    public:
        static std::shared_ptr<ImageData> load(const std::string& filepath, bool isSRGB) {
            std::string key = filepath + (isSRGB ? "_srgb" : "_linear");

            if (m_cache.contains(key)) {
                return m_cache[key];
            }

            auto imgData = std::make_shared<ImageData>();

            if (filepath.ends_with(".exr")) {
                try {
                    Imf::RgbaInputFile file(filepath.c_str());
                    Imath::Box2i dw = file.dataWindow();

                    imgData->width = dw.max.x - dw.min.x + 1;
                    imgData->height = dw.max.y - dw.min.y + 1;
                    imgData->channels = 4;

                    Imf::Array2D<Imf::Rgba> pixels(imgData->height, imgData->width);

                    file.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * imgData->width, 1, imgData->width);
                    file.readPixels(dw.min.y, dw.max.y);

                    int totalValues = imgData->width * imgData->height * imgData->channels;
                    imgData->data.resize(totalValues);

                    for (int y = 0; y < imgData->height; ++y) {
                        int dstY = imgData->height - 1 - y;
                        for (int x = 0; x < imgData->width; ++x) {
                            const Imf::Rgba& p = pixels[y][x];
                            int idx = (dstY * imgData->width + x) * 4;

                            imgData->data[idx + 0] = p.r;
                            imgData->data[idx + 1] = p.g;
                            imgData->data[idx + 2] = p.b;
                            imgData->data[idx + 3] = p.a;
                        }
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("TextureCache: Failed to load EXR " + filepath + " - " + e.what());
                }
            }
            else {
                stbi_set_flip_vertically_on_load(true);
                unsigned char* rawData = stbi_load(filepath.c_str(), &imgData->width, &imgData->height, &imgData->channels, 0);

                if (!rawData) {
                    throw std::runtime_error("TextureCache: Failed to load image " + filepath);
                }

                int totalValues = imgData->width * imgData->height * imgData->channels;
                imgData->data.resize(totalValues);

                auto toLinear = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };

                for (int i = 0; i < totalValues; ++i) {
                    float val = rawData[i] / 255.0f;
                    if (isSRGB && (imgData->channels <= 3 || i % imgData->channels < 3)) {
                        val = toLinear(val);
                    }
                    imgData->data[i] = val;
                }

                stbi_image_free(rawData);
            }

            m_cache[key] = imgData;

            return imgData;
        }

        static void clear() { m_cache.clear(); }

    private:
        inline static std::unordered_map<std::string, std::shared_ptr<ImageData>> m_cache;
    };
}