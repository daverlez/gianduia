#include <filesystem>
#include <vector>
#include <memory>
#include <stdexcept>

#include "gianduia/core/film.h"
#include "gianduia/core/emitter.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/fileResolver.h"
#include "gianduia/math/distribution.h"

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

            m_bitmap = Film(absPath.string());
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

            float mapPdf;
            Point2f uv = m_distribution->SampleContinuous(sample_val, &mapPdf);

            if (mapPdf == 0.0f) {
                pdf_val = 0.0f;
                return Color3f(0.0f);
            }

            float theta = uv.y() * M_PI;
            float phi = uv.x() * 2.0f * M_PI;

            Vector3f wi = SphericalDirection(Point2f(theta, phi));

            info.p = ref.p + wi * 1e5f;
            info.n = Normal3f(-wi);
            info.time = ref.time;

            shadowRay = Ray(ref.p, wi);
            shadowRay.time = ref.time;

            float sinTheta = std::sin(theta);
            if (sinTheta <= 0.0f) {
                pdf_val = 0.0f;
                return Color3f(0.0f);
            }

            pdf_val = mapPdf / (2.0f * M_PI * M_PI * sinTheta);

            return eval(info, wi) / pdf_val;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const override {
            Vector3f wi = Normalize(info.p - ref.p);
            Point2f spherical = SphericalCoordinates(wi);
            float theta = spherical.x();
            float phi = spherical.y();
            float sinTheta = std::sin(theta);

            if (sinTheta <= 0.0f)
                return 0.0f;

            float u = phi * M_1_PI * 0.5f;
            float v = theta * M_1_PI;

            float mapPdf = m_distribution->Pdf(Point2f(u, v));
            return mapPdf / (2.0f * M_PI * M_PI * sinTheta);
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
        void buildPDFs() {
            const int rows = m_bitmap.height();
            const int cols = m_bitmap.width();

            std::vector<float> imgData(rows * cols);

            for (int i = 0; i < rows; i++) {
                float sinTheta = std::sin(M_PI * (i + 0.5f) / rows);
                for (int j = 0; j < cols; j++) {
                    imgData[i * cols + j] = m_bitmap.getPixel(j, i).luminance() * sinTheta;
                }
            }

            m_distribution = std::make_unique<Distribution2D>(imgData.data(), cols, rows);
        }

    protected:
        Film m_bitmap = Film(0, 0);
        std::unique_ptr<Distribution2D> m_distribution;
        std::string m_relPath;
        float m_strength;
    };

    GND_REGISTER_CLASS(EnvironmentMap, "environment")
}