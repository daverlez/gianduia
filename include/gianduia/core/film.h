#pragma once

#include <memory>
#include <atomic>
#include <string>

#include "gianduia/core/filter.h"
#include "gianduia/math/color.h"
#include "gianduia/core/image2d.h"

namespace gnd {

    struct AOVRecord {
        Color3f albedo{0.0f};
        Normal3f normal{0.0f};
        float depth{-1.0f};
        float roughness{0.0f};
        float metallic{0.0f};
    };

    class Film {
    public:
        Film(int width, int height, std::shared_ptr<Filter> filter = nullptr);
        Film(const std::string& exrAbsolutePath);

        void addSample(const Point2f& pFilm, const Color3f& L,
                       const AOVRecord& pAOVs) {
            if (L.hasNaNs()) return;

            Color3f albedo = pAOVs.albedo;
            Normal3f normal = pAOVs.normal;
            float depth = pAOVs.depth;
            float roughness = pAOVs.roughness;
            float metallic = pAOVs.metallic;

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

                        atomicAdd(m_accumData[idx].nrm_x, normal.x() * weight);
                        atomicAdd(m_accumData[idx].nrm_y, normal.y() * weight);
                        atomicAdd(m_accumData[idx].nrm_z, normal.z() * weight);

                        atomicAdd(m_accumData[idx].depth, depth * weight);
                        atomicAdd(m_accumData[idx].roughness, roughness * weight);
                        atomicAdd(m_accumData[idx].metallic, metallic * weight);
                    }
                }
            }
        }

        void resolve();
        void clear();

        void setPixel(int x, int y, const Color3f& color) {
            m_radiance.setPixel(x, y, color);
        }

        const Color3f& getPixel(int x, int y) const {
            return m_radiance.getPixel(x, y);
        }

        Color3f getPixelBilinear(float u, float v) const;

        const std::shared_ptr<Filter> getFilter() const { return m_filter; }

        void savePNG() const;
        void saveEXR() const;
        void savePNG(const std::string& absolutePath) const;
        void saveEXR(const std::string& absolutePath) const;

        int width() const { return m_width; }
        int height() const { return m_height; }
        float aspectRatio() const { return (float)m_width / (float)m_height; }

        const Image2D<Color3f>& getRadiance() const { return m_radiance; }
        const Image2D<Color3f>& getAlbedo() const { return m_albedo; }
        const Image2D<Normal3f>& getNormal() const { return m_normal; }
        const Image2D<float>& getDepth() const { return m_depth; }
        const Image2D<float>& getRoughness() const { return m_roughness; }
        const Image2D<float>& getMetallic() const { return m_metallic; }

        Image2D<Color3f>& getRadiance() { return m_radiance; }
        Image2D<Color3f>& getAlbedo() { return m_albedo; }
        Image2D<Normal3f>& getNormal() { return m_normal; }
        Image2D<float>& getDepth() { return m_depth; }
        Image2D<float>& getRoughness() { return m_roughness; }
        Image2D<float>& getMetallic() { return m_metallic; }

    private:
        struct AccumPixel {
            std::atomic<float> r{0.0f}, g{0.0f}, b{0.0f}, weight{0.0f};
            std::atomic<float> alb_r{0.0f}, alb_g{0.0f}, alb_b{0.0f};
            std::atomic<float> nrm_x{0.0f}, nrm_y{0.0f}, nrm_z{0.0f};
            std::atomic<float> depth;
            std::atomic<float> roughness;
            std::atomic<float> metallic;
        };

        inline void atomicAdd(std::atomic<float>& target, float value) {
            float current = target.load(std::memory_order_relaxed);
            while (!target.compare_exchange_weak(current, current + value,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));
        }

        int m_width, m_height;
        std::unique_ptr<AccumPixel[]> m_accumData;
        std::shared_ptr<Filter> m_filter;

        Image2D<Color3f> m_radiance;
        Image2D<Color3f> m_albedo;
        Image2D<Normal3f> m_normal;
        Image2D<float> m_depth;
        Image2D<float> m_roughness;
        Image2D<float> m_metallic;
    };

}