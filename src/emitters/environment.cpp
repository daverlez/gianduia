#include <filesystem>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "gianduia/core/bitmap.h"
#include "gianduia/core/emitter.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/fileResolver.h"

namespace gnd {
    class EnvironmentMap : public Emitter {
    public:
        EnvironmentMap(const PropertyList &props) : Emitter(EMITTER_ENV) {
            m_relPath = props.getString("filename");
            std::filesystem::path absPath = FileResolver::resolve(m_relPath);

            if (!std::filesystem::is_regular_file(absPath))
                throw std::runtime_error("EnvMap: Invalid environment map path.");
            if (absPath.extension() != ".exr" && absPath.extension() != "exr")
                throw std::runtime_error("EnvMap: Invalid environment map extension.");

            m_bitmap = Bitmap(absPath.string());
            m_strength = props.getFloat("strength", 1.0f);

            this->buildPDFs();
        }

        virtual Color3f eval(const SurfaceInteraction& isect, const Vector3f& w) const override {
            Point2f spherical = SphericalCoordinates(w);
            float theta = spherical.x();
            float phi = spherical.y();

            float u = phi * M_1_PI * 0.5f;
            float v = theta * M_1_PI;

            return m_strength * m_bitmap.getPixelBilinear(u, v);
        }

        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample_val,
                               SurfaceInteraction& info, float& pdf_val, Ray& shadowRay) const override {
            const int rows = m_bitmap.height();
            const int cols = m_bitmap.width();

            int v = sampleDiscreteDistribution(m_thetaCdf, sample_val.x());
            int u = sampleDiscreteDistributionMatrix(m_phiCdf, cols, v, sample_val.y());

            float cdf_theta_prev = (v > 0) ? m_thetaCdf[v - 1] : 0.0f;
            float delta_theta = m_thetaCdf[v] - cdf_theta_prev;
            float dv = (delta_theta > 0.0f) ? (sample_val.x() - cdf_theta_prev) / delta_theta : 0.5f;

            float cdf_phi_prev = (u > 0) ? m_phiCdf[v * cols + u - 1] : 0.0f;
            float delta_phi = m_phiCdf[v * cols + u] - cdf_phi_prev;
            float du = (delta_phi > 0.0f) ? (sample_val.y() - cdf_phi_prev) / delta_phi : 0.5f;

            dv = std::clamp(dv, 0.0f, 1.0f);
            du = std::clamp(du, 0.0f, 1.0f);

            dv = std::clamp(dv, 0.0f, 1.0f);
            du = std::clamp(du, 0.0f, 1.0f);

            float theta = M_PI * (v + dv) / rows;
            float phi = 2.0f * M_PI * (u + du) / cols;

            Vector3f wi = SphericalDirection(Point2f(theta, phi));

            info.p = ref.p + wi * 1e5f;
            info.n = Normal3f(-wi);

            shadowRay = Ray(ref.p, wi);

            pdf_val = this->pdf(ref, info);

            if (pdf_val <= 0.0f)
                return Color3f(0.0f);

            return eval(info, wi) / pdf_val;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const override {
            const int rows = m_bitmap.height();
            const int cols = m_bitmap.width();

            Vector3f wi = Normalize(info.p - ref.p);
            Point2f spherical = SphericalCoordinates(wi);
            float theta = spherical.x();
            float phi = spherical.y();

            int u = std::floor(phi * M_1_PI * 0.5f * cols);
            int v = std::floor(theta * M_1_PI * rows);

            u = std::clamp(u, 0, cols - 1);
            v = std::clamp(v, 0, rows - 1);

            float sinTheta = std::sin(theta);
            if (sinTheta <= 0.0f)
                return 0.0f;

            float jacobian = (rows * cols) / (sinTheta * 2.0f * M_PI * M_PI);
            return m_discretePdf2d[v * cols + u] * jacobian;
        }

        virtual bool isInfiniteAreaLight() const override { return true; }

        virtual std::string toString() const override {
            return std::format(
                "EnvironmentMap[\n"
                        "  path = {}\n"
                        "  strength = {}\n"
                        "]",
                        m_relPath,
                        m_strength);
        }

    private:
        static int sampleDiscreteDistribution(const std::vector<float>& cdf, float sample_val) {
            if (sample_val <= 0.0f) return 0;
            if (sample_val >= 1.0f) return cdf.size() - 1;

            auto it = std::upper_bound(cdf.begin(), cdf.end(), sample_val);
            return std::clamp<int>(std::distance(cdf.begin(), it), 0, cdf.size() - 1);
        }

        static int sampleDiscreteDistributionMatrix(const std::vector<float>& cdf, int cols, int row, float sample_val) {
            int offset = row * cols;
            if (sample_val <= 0.0f) return 0;
            if (sample_val >= 1.0f) return cols - 1;

            auto begin_it = cdf.begin() + offset;
            auto end_it = begin_it + cols;
            
            auto it = std::upper_bound(begin_it, end_it, sample_val);
            return std::clamp<int>(std::distance(begin_it, it), 0, cols - 1);
        }

        void buildPDFs() {
            const int rows = m_bitmap.height();
            const int cols = m_bitmap.width();
            const int totalPixels = rows * cols;

            m_discretePdf2d.resize(totalPixels, 0.0f);
            m_thetaMarginalPdf.assign(rows, 0.0f);
            m_thetaCdf.assign(rows, 0.0f);
            m_phiConditionalPdf.assign(totalPixels, 0.0f);
            m_phiCdf.assign(totalPixels, 0.0f);

            float lumSum = 0.0f;
            for (int i = 0; i < rows; i++) {
                float sinTheta = std::sin(M_PI * (i + 0.5f) / rows);
                for (int j = 0; j < cols; j++) {
                    lumSum += m_bitmap.getPixel(j, i).luminance() * sinTheta;
                }
            }

            if (lumSum == 0.0f)
                throw std::runtime_error("Environment map is completely black/invalid.");

            for (int i = 0; i < rows; i++) {
                float sinTheta = std::sin(M_PI * (i + 0.5f) / rows);
                for (int j = 0; j < cols; j++) {
                    m_discretePdf2d[i * cols + j] = (m_bitmap.getPixel(j, i).luminance() * sinTheta) / lumSum;
                }
            }

            float F_theta = 0.0f;
            for (int i = 0; i < rows; ++i) {
                for (int j = 0; j < cols; ++j) {
                    m_thetaMarginalPdf[i] += m_discretePdf2d[i * cols + j];
                }
                F_theta += m_thetaMarginalPdf[i];
                m_thetaCdf[i] = F_theta;
            }
            m_thetaCdf[rows - 1] = 1.0f;

            for (int i = 0; i < rows; ++i) {
                float F_phi = 0.0f;
                for (int j = 0; j < cols; ++j) {
                    if (m_thetaMarginalPdf[i] > 0.0f) {
                        m_phiConditionalPdf[i * cols + j] = m_discretePdf2d[i * cols + j] / m_thetaMarginalPdf[i];
                    } else {
                        m_phiConditionalPdf[i * cols + j] = 0.0f;
                    }
                    F_phi += m_phiConditionalPdf[i * cols + j];
                    m_phiCdf[i * cols + j] = F_phi;
                }
                m_phiCdf[i * cols + cols - 1] = 1.0f;
            }
        }

    protected:
        Bitmap m_bitmap = Bitmap(0, 0);
        std::vector<float> m_discretePdf2d;
        std::vector<float> m_thetaMarginalPdf;
        std::vector<float> m_thetaCdf;
        std::vector<float> m_phiConditionalPdf;
        std::vector<float> m_phiCdf;
        std::string m_relPath;
        float m_strength;
    };

    GND_REGISTER_CLASS(EnvironmentMap, "environment")
}