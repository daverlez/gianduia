#include <filesystem>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cmath>

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

            float rotationDeg = props.getFloat("rotation", 0.0f);
            m_rotationRad = Radians(rotationDeg);

            this->buildPDFs();
        }

        virtual Color3f eval(const SurfaceInteraction& isect, const Vector3f& w) const override {
            float theta = std::acos(Clamp(w.z(), -1.0f, 1.0f));
            float phiRot = std::atan2(w.x(), -w.y());
            phiRot -= m_rotationRad;

            float u = phiRot * Inv2Pi + 0.5f;
            u -= std::floor(u);
            float v = theta * InvPi;

            return m_strength * m_bitmap.getPixelBilinear(u, v);
        }

        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample_val,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const override {
            float mapPdf;
            Point2f uv = m_distribution->SampleContinuous(sample_val, &mapPdf);

            if (mapPdf == 0.0f) {
                pdf = 0.0f;
                return Color3f(0.0f);
            }

            float theta = uv.y() * Pi;
            float phiRot = (uv.x() - 0.5f) * TwoPi;
            float phi = phiRot + m_rotationRad;

            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vector3f wi(
                sinTheta * std::sin(phi),
               -sinTheta * std::cos(phi),
                cosTheta
            );

            if (sinTheta <= 0.0f) {
                pdf = 0.0f;
                return Color3f(0.0f);
            }

            info.p = ref.p + wi * 1e5f;
            info.n = Normal3f(-wi);
            info.time = ref.time;

            shadowRay = Ray(ref.p, wi);
            shadowRay.time = ref.time;

            pdf = mapPdf / (TwoPi * Pi * sinTheta);

            return eval(info, wi) / pdf;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const override {
            Vector3f wi = Normalize(info.p - ref.p);
            float theta = std::acos(Clamp(wi.z(), -1.0f, 1.0f));
            float sinTheta = std::sin(theta);

            if (sinTheta <= 0.0f)
                return 0.0f;

            float phiRot = std::atan2(wi.x(), -wi.y());
            phiRot -= m_rotationRad;

            float u = phiRot * Inv2Pi + 0.5f;
            u -= std::floor(u);
            float v = theta * InvPi;

            float mapPdf = m_distribution->Pdf(Point2f(u, v));
            return mapPdf / (TwoPi * Pi * sinTheta);
        }

        virtual Color3f samplePhoton(const Point2f& uPos, const Point2f& uDir, float time, Ray& photonRay) const override {
            std::cerr << "Warning: samplePhoton not yet supported for infinite lights!" << std::endl;
            return Color3f(0.0f);
        }

        virtual bool isInfiniteAreaLight() const override { return true; }

        virtual std::string toString() const override {
            return std::format(
                "EnvironmentMap[\n"
                "  path = {}\n"
                "  strength = {}\n"
                "  rotationRad = {}\n"
                "]",
                m_relPath,
                m_strength,
                m_rotationRad);
        }

    private:
        void buildPDFs() {
            const int rows = m_bitmap.height();
            const int cols = m_bitmap.width();

            std::vector<float> imgData(rows * cols);

            for (int i = 0; i < rows; i++) {
                float sinTheta = std::sin(Pi * (i + 0.5f) / rows);
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
        float m_rotationRad;
    };

    GND_REGISTER_CLASS(EnvironmentMap, "environment")
}