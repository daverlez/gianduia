#pragma once

#include <gianduia/textures/texture.h>
#include <gianduia/math/worley.h>
#include <algorithm>
#include <string>

namespace gnd {

    enum class CellularMode {
        Distance,
        Cracks
    };

    template<ProceduralTextureValue T>
    class CellularTexture : public Texture<T> {
    public:
        explicit CellularTexture(const PropertyList& props) {
            if constexpr (std::same_as<T, float>) {
                m_value1 = props.getFloat("value1", 0.0f);
                m_value2 = props.getFloat("value2", 1.0f);
            } else {
                m_value1 = props.getColor("value1", Color3f(0.0f));
                m_value2 = props.getColor("value2", Color3f(1.0f));
            }

            m_scale = props.getFloat("scale", 1.0f);

            m_modeStr = props.getString("mode", "distance");
            if (m_modeStr == "cracks") {
                m_mode = CellularMode::Cracks;
            } else {
                m_mode = CellularMode::Distance;
                m_modeStr = "distance";
            }

            m_metricStr = props.getString("metric", "euclidean");
            if (m_metricStr == "manhattan") {
                m_metric = WorleyMetric::Manhattan;
            } else if (m_metricStr == "chebyshev") {
                m_metric = WorleyMetric::Chebyshev;
            } else {
                m_metric = WorleyMetric::Euclidean;
                m_metricStr = "euclidean";
            }
        }

        T evaluate(const SurfaceInteraction& isect) const override {
            WorleyResult wr = Worley::noise(isect.p / m_scale, m_metric);

            float noiseVal = 0.0f;
            if (m_mode == CellularMode::Cracks)
                noiseVal = wr.f2 - wr.f1;
            else
                noiseVal = wr.f1;
            noiseVal = std::clamp(noiseVal, 0.0f, 1.0f);

            return Lerp(noiseVal, m_value1, m_value2);
        }

        std::string toString() const override {
            return std::format(
                "CellularTexture([\n"
                "  mode = {}\n"
                "  metric = {}\n"
                "  scale = {}\n"
                "]",
                m_modeStr, m_metricStr, m_scale
            );
        }

    private:
        T m_value1;
        T m_value2;
        float m_scale;
        CellularMode m_mode;
        std::string m_modeStr;
        WorleyMetric m_metric;
        std::string m_metricStr;
    };
}