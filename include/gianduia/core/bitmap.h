#pragma once

#include <memory>
#include <atomic>
#include <vector>

#include <gianduia/core/filter.h>
#include <gianduia/math/color.h>

namespace gnd {

    class Bitmap {
    public:
        Bitmap(int width, int height, std::shared_ptr<Filter> filter = nullptr)
            : m_width(width), m_height(height), m_filter(filter) {
            m_pixels.resize(width * height, Color3f(0.0f));
            m_accumData = std::make_unique<AccumPixel[]>(width * height);

            m_albedo.resize(width * height, Color3f(0.0f));
            m_normal.resize(width * height, Color3f(0.0f));

            if (!m_filter) m_filter = std::make_shared<GaussianFilter>(PropertyList());
        }

        Bitmap(const std::string& exrAbsolutePath);

        /// Adds a sample to the accumulated data
        void addSample(const Point2f& pFilm, const Color3f& L, const Color3f& albedo = Color3f(0.0f), const Color3f& normal = Color3f(0.0f)) {
            if (L.hasNaNs()) return;

            Vector2f radius = m_filter->getRadius();

            int x0 = std::max(0, (int)std::ceil(pFilm.x() - radius.x() - 0.5f));
            int x1 = std::min(m_width - 1, (int)std::floor(pFilm.x() + radius.x() - 0.5f));
            int y0 = std::max(0, (int)std::ceil(pFilm.y() - radius.y() - 0.5f));
            int y1 = std::min(m_height - 1, (int)std::floor(pFilm.y() + radius.y() - 0.5f));

            for (int py = y0; py <= y1; ++py) {
                for (int px = x0; px <= x1; ++px) {
                    Point2f offset(pFilm.x() - (px + 0.5f), pFilm.y() - (py + 0.5f));
                    float weight = m_filter->evaluate(offset);

                    if (weight > 0.0f) {
                        int idx = py * m_width + px;
                        atomicAdd(m_accumData[idx].r, L.r() * weight);
                        atomicAdd(m_accumData[idx].g, L.g() * weight);
                        atomicAdd(m_accumData[idx].b, L.b() * weight);
                        atomicAdd(m_accumData[idx].weight, weight);

                        atomicAdd(m_accumData[idx].alb_r, albedo.r() * weight);
                        atomicAdd(m_accumData[idx].alb_g, albedo.g() * weight);
                        atomicAdd(m_accumData[idx].alb_b, albedo.b() * weight);
                        atomicAdd(m_accumData[idx].nrm_x, normal.r() * weight);
                        atomicAdd(m_accumData[idx].nrm_y, normal.g() * weight);
                        atomicAdd(m_accumData[idx].nrm_z, normal.b() * weight);
                    }
                }
            }
        }

        /// Transfers accumulated data to pixel buffer
        void resolve() {
            for (int i = 0; i < m_width * m_height; ++i) {
                float w = m_accumData[i].weight.load(std::memory_order_relaxed);
                if (w > 0.0f) {
                    float invW = 1.0f / w;
                    m_pixels[i] = Color3f(
                        m_accumData[i].r.load(std::memory_order_relaxed) * invW,
                        m_accumData[i].g.load(std::memory_order_relaxed) * invW,
                        m_accumData[i].b.load(std::memory_order_relaxed) * invW
                    );
                    m_albedo[i] = Color3f(
                        m_accumData[i].alb_r.load(std::memory_order_relaxed) * invW,
                        m_accumData[i].alb_g.load(std::memory_order_relaxed) * invW,
                        m_accumData[i].alb_b.load(std::memory_order_relaxed) * invW
                    );
                    m_normal[i] = Color3f(
                        m_accumData[i].nrm_x.load(std::memory_order_relaxed) * invW,
                        m_accumData[i].nrm_y.load(std::memory_order_relaxed) * invW,
                        m_accumData[i].nrm_z.load(std::memory_order_relaxed) * invW
                    );
                } else {
                    m_pixels[i] = Color3f(0.0f);
                    m_albedo[i] = Color3f(0.0f);
                    m_normal[i] = Color3f(0.0f);
                }
            }
        }

        void clear() {
            std::ranges::fill(m_pixels, Color3f(0.0f));
            for (int i = 0; i < m_width * m_height; ++i) {
                m_accumData[i].r.store(0.0f, std::memory_order_relaxed);
                m_accumData[i].g.store(0.0f, std::memory_order_relaxed);
                m_accumData[i].b.store(0.0f, std::memory_order_relaxed);
                m_accumData[i].weight.store(0.0f, std::memory_order_relaxed);
            }
        }

        void setPixel(int x, int y, const Color3f& color) {
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;

            m_pixels[y * m_width + x] = color;
        }

        const Color3f& getPixel(int x, int y) const {
            return m_pixels[y * m_width + x];
        }

        float* data() {
            if (m_pixels.empty()) return nullptr;
            return reinterpret_cast<float*>(m_pixels.data());
        }

        float* albedoData() {
            if (m_albedo.empty()) return nullptr;
            return reinterpret_cast<float*>(m_albedo.data());
        }
        float* normalData() {
            if (m_normal.empty()) return nullptr;
            return reinterpret_cast<float*>(m_normal.data());
        }

        Color3f getPixelBilinear(float u, float v) const {
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

        const std::shared_ptr<Filter> getFilter() const {
            return m_filter;
        }

        void savePNG() const;
        void saveEXR() const;

        int width() const { return m_width; }
        int height() const { return m_height; }
        float aspectRatio() const { return (float)m_width / (float)m_height; }

    private:
        struct AccumPixel {
            std::atomic<float> r{0.0f}, g{0.0f}, b{0.0f}, weight{0.0f};
            std::atomic<float> alb_r{0.0f}, alb_g{0.0f}, alb_b{0.0f};
            std::atomic<float> nrm_x{0.0f}, nrm_y{0.0f}, nrm_z{0.0f};
        };

        inline void atomicAdd(std::atomic<float>& target, float value) {
            float current = target.load(std::memory_order_relaxed);
            while (!target.compare_exchange_weak(current, current + value,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));
        }

        int m_width, m_height;
        std::vector<Color3f> m_pixels;
        std::unique_ptr<AccumPixel[]> m_accumData;
        std::shared_ptr<Filter> m_filter;

        std::vector<Color3f> m_albedo;
        std::vector<Color3f> m_normal;
    };

}
