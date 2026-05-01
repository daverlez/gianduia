#include "gianduia/core/film.h"
#include "gianduia/core/fileResolver.h"

#include <iostream>
#include <exception>

#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <Imath/ImathBox.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace gnd {

    Film::Film(int width, int height, std::shared_ptr<Filter> filter)
        : m_width(width), m_height(height), m_filter(filter) {

        m_accumData = std::make_unique<AccumPixel[]>(width * height);

        m_radiance.resize(width, height, Color3f(0.0f));
        m_albedo.resize(width, height, Color3f(0.0f));
        m_normal.resize(width, height, Normal3f(0.0f));
        m_depth.resize(width, height, -1.0f);

        if (!m_filter) m_filter = std::make_shared<GaussianFilter>(PropertyList());
    }

    Film::Film(const std::string& exrAbsolutePath) {
        if (!m_radiance.loadEXR(exrAbsolutePath)) {
            throw std::runtime_error("Film: Failed to load EXR from " + exrAbsolutePath);
        }

        m_width = m_radiance.width();
        m_height = m_radiance.height();

        m_accumData = std::make_unique<AccumPixel[]>(m_width * m_height);
        m_albedo.resize(m_width, m_height, Color3f(0.0f));
        m_normal.resize(m_width, m_height, Normal3f(0.0f));
        m_depth.resize(m_width, m_height, -1.0f);

        m_filter = std::make_shared<GaussianFilter>(PropertyList());
    }

    void Film::resolve() {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                int idx = y * m_width + x;
                float w = m_accumData[idx].weight.load(std::memory_order_relaxed);

                if (w > 0.0f) {
                    float invW = 1.0f / w;

                    m_radiance.setPixel(x, y, Color3f(
                        m_accumData[idx].r.load(std::memory_order_relaxed) * invW,
                        m_accumData[idx].g.load(std::memory_order_relaxed) * invW,
                        m_accumData[idx].b.load(std::memory_order_relaxed) * invW
                    ));

                    m_albedo.setPixel(x, y, Color3f(
                        m_accumData[idx].alb_r.load(std::memory_order_relaxed) * invW,
                        m_accumData[idx].alb_g.load(std::memory_order_relaxed) * invW,
                        m_accumData[idx].alb_b.load(std::memory_order_relaxed) * invW
                    ));

                    m_normal.setPixel(x, y, Normal3f(
                        m_accumData[idx].nrm_x.load(std::memory_order_relaxed) * invW,
                        m_accumData[idx].nrm_y.load(std::memory_order_relaxed) * invW,
                        m_accumData[idx].nrm_z.load(std::memory_order_relaxed) * invW
                    ));

                    float d = m_accumData[idx].depth.load(std::memory_order_relaxed) * invW;
                    m_depth.setPixel(x, y, d);
                } else {
                    m_radiance.setPixel(x, y, Color3f(0.0f));
                    m_albedo.setPixel(x, y, Color3f(0.0f));
                    m_normal.setPixel(x, y, Normal3f(0.0f));
                }
            }
        }
    }

    void Film::clear() {
        m_radiance.clear();
        m_albedo.clear();
        m_normal.clear();

        for (int i = 0; i < m_width * m_height; ++i) {
            m_accumData[i].r.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].g.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].b.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].weight.store(0.0f, std::memory_order_relaxed);

            m_accumData[i].alb_r.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].alb_g.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].alb_b.store(0.0f, std::memory_order_relaxed);

            m_accumData[i].nrm_x.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].nrm_y.store(0.0f, std::memory_order_relaxed);
            m_accumData[i].nrm_z.store(0.0f, std::memory_order_relaxed);

            m_accumData[i].depth.store(0.0f, std::memory_order_relaxed);
        }
    }

    Color3f Film::getPixelBilinear(float u, float v) const {
        if (m_width == 0 || m_height == 0) return Color3f(0.0f);

        float x = u * m_width - 0.5f;
        float y = v * m_height - 0.5f;

        int x0 = std::floor(x);
        int y0 = std::floor(y);

        float dx = x - x0;
        float dy = y - y0;

        int px0 = (x0 % m_width + m_width) % m_width;
        int px1 = (px0 + 1) % m_width;

        int py0 = std::clamp(y0, 0, m_height - 1);
        int py1 = std::clamp(y0 + 1, 0, m_height - 1);

        Color3f c00 = getPixel(px0, py0);
        Color3f c10 = getPixel(px1, py0);
        Color3f c01 = getPixel(px0, py1);
        Color3f c11 = getPixel(px1, py1);

        Color3f c0 = c00 * (1.0f - dx) + c10 * dx;
        Color3f c1 = c01 * (1.0f - dx) + c11 * dx;

        return c0 * (1.0f - dy) + c1 * dy;
    }

    void Film::savePNG() const {
        std::vector<uint8_t> data(m_width * m_height * 3);

        const Color3f* pixels = m_radiance.data();

        int index = 0;
        for (int i = 0; i < m_width * m_height; ++i) {
            Color3f linearColor = pixels[i];
            Color3f srgbColor = linearColor.toSRGB();

            Color3f final = srgbColor * 255.0f + Color3f(0.5);
            final = final.clamp(0.0f, 255.0f);

            data[index++] = static_cast<uint8_t>(final.r());
            data[index++] = static_cast<uint8_t>(final.g());
            data[index++] = static_cast<uint8_t>(final.b());
        }

        std::string filename = FileResolver::getOutputPath().string() + "/" + FileResolver::getOutputName() + ".png";
        int success = stbi_write_png(filename.c_str(), m_width, m_height, 3, data.data(), m_width * 3);

        if (!success) {
            std::cerr << "Error while saving image: " << filename << std::endl;
        } else {
            std::cout << "PNG image saved successfully: " << filename << std::endl;
        }
    }

    void Film::saveEXR() const {
        std::string basePath = FileResolver::getOutputPath().string() + "/" + FileResolver::getOutputName();

        m_radiance.saveEXR(basePath + ".exr");
        //m_albedo.saveEXR(basePath + "_albedo.exr");
        //m_normal.saveEXR(basePath + "_normal.exr");
    }

}