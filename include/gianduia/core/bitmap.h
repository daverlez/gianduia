#pragma once

#include <gianduia/core/filter.h>
#include <gianduia/math/color.h>
#include <vector>

namespace gnd {

    class Bitmap {
    public:
        Bitmap(int width, int height, std::shared_ptr<Filter> filter = nullptr)
            : m_width(width), m_height(height), m_filter(filter) {
            m_pixels.resize(width * height, Color3f(0.0f));
            m_accumData = std::make_unique<AccumPixel[]>(width * height);

            if (!m_filter) m_filter = std::make_shared<GaussianFilter>(PropertyList());
        }

        /// Adds a sample to the accumulated data
        void addSample(const Point2f& pFilm, const Color3f& L) {
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
                } else {
                    m_pixels[i] = Color3f(0.0f);
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

        const std::shared_ptr<Filter> getFilter() const {
            return m_filter;
        }

        const float* data() const {
            if (m_pixels.empty()) return nullptr;
            return reinterpret_cast<const float*>(m_pixels.data());
        }

        void savePNG() const;

        int width() const { return m_width; }
        int height() const { return m_height; }
        float aspectRatio() const { return (float)m_width / (float)m_height; }

    private:
        struct AccumPixel {
            std::atomic<float> r{0.0f}, g{0.0f}, b{0.0f}, weight{0.0f};
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
    };

}
