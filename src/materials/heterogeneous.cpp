#include <gianduia/materials/medium.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/sampler.h>
#include <gianduia/math/color.h>
#include <gianduia/core/fileResolver.h>

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/Statistics.h>

#include <cmath>
#include <algorithm>

namespace gnd {

    class HeterogeneousMedium : public Medium {
    public:
        HeterogeneousMedium(const PropertyList& props) {
            m_base_sigma_a = props.getColor("sigma_a", Color3f(0.1f));
            m_base_sigma_s = props.getColor("sigma_s", Color3f(0.1f));
            m_base_sigma_t = m_base_sigma_a + m_base_sigma_s;
            m_g = props.getFloat("g", 0.0f);

            m_worldToLocal = props.getTransform("toLocal", Transform());

            std::string filename = props.getString("filename", "");
            std::string gridName = props.getString("gridName", "density");
            m_densityScale = props.getFloat("densityScale", 1.0f);

            std::string tempGridName = props.getString("temperatureGrid", "temperature");
            m_temperatureScale = props.getFloat("temperatureScale", 1.0f);
            m_temperatureOffset = props.getFloat("temperatureOffset", 0.0f);
            m_emissionScale = props.getFloat("emissionScale", 10.0f);

            std::string absPath = FileResolver::resolve(filename);

            openvdb::initialize();
            openvdb::io::File file(absPath);
            file.open();
            
            openvdb::GridBase::Ptr baseGrid;
            openvdb::GridBase::Ptr tempGrid;
            for (auto nameIter = file.beginName(); nameIter != file.endName(); ++nameIter) {
                if (nameIter.gridName() == gridName) {
                    baseGrid = file.readGrid(nameIter.gridName());
                }
                if (nameIter.gridName() == tempGridName) {
                    tempGrid = file.readGrid(nameIter.gridName());
                }
            }
            file.close();

            if (!baseGrid) throw std::runtime_error("HeterogeneousMedium: base VDB grid not found!");

            m_densityGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
            if (tempGrid) {
                m_temperatureGridName = tempGrid->getName();
                m_temperatureGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(tempGrid);
            }

            auto extrema = openvdb::tools::minMax(m_densityGrid->tree());
            float maxVal = extrema.max();

            if (!std::isfinite(maxVal)) {
                // VDB grid contains non-finite values. This will cause delta tracking to get stuck.
                throw std::runtime_error("HeterogeneousMedium: VDB grid has non-finite values (inf/nan).");
            }

            float max_color_t = std::max({m_base_sigma_t.r(), m_base_sigma_t.g(), m_base_sigma_t.b()});
            m_majorant = maxVal * m_densityScale * max_color_t;
            m_invMajorant = m_majorant > 0.0f ? 1.0f / m_majorant : 0.0f;
        }

        Color3f Tr(const Ray& ray, Sampler& sampler) const override {
            if (m_majorant <= 0.0f || std::isinf(ray.tMax)) return Color3f(1.0f);

            float tMax = ray.tMax;
            float t = ray.tMin;
            Color3f tr(1.0f);

            auto accessor = m_densityGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> gridSampler(accessor, m_densityGrid->transform());

            // 1. Calcolo del raggio nell'Index Space di OpenVDB FUORI dal ciclo
            Point3f localO = m_worldToLocal(ray.o);
            Point3f localTarget = m_worldToLocal(ray.o + ray.d);

            openvdb::Vec3d vdbO(localO.x(), localO.y(), localO.z());
            openvdb::Vec3d vdbTarget(localTarget.x(), localTarget.y(), localTarget.z());

            const openvdb::math::Transform& gridTrans = m_densityGrid->transform();
            openvdb::Vec3d indexO = gridTrans.worldToIndex(vdbO);
            openvdb::Vec3d indexD = gridTrans.worldToIndex(vdbTarget) - indexO;

            while (true) {
                t += -std::log(1.0f - sampler.next1D()) * m_invMajorant;
                if (t >= tMax) break;

                openvdb::Vec3d pIndex = indexO + indexD * (double)t;
                float localDensity = gridSampler.isSample(pIndex) * m_densityScale;

                if (localDensity > 0.0f) {
                    Color3f local_sigma_t = m_base_sigma_t * localDensity;
                    tr *= (Color3f(1.0f) - local_sigma_t * m_invMajorant);

                    float maxTr = std::max(tr.r(), std::max(tr.g(), tr.b()));

                    if (maxTr < 0.1f) {
                        float q = std::max(0.05f, 1.0f - maxTr);
                        if (sampler.next1D() < q) return Color3f(0.0f);
                        tr /= (1.0f - q);
                    }
                }
            }
            return tr;
        }

        virtual Color3f Le(const Point3f& pWorld) const override {
            if (!m_temperatureGrid || m_emissionScale <= 0.0f) return Color3f(0.0f);

            auto accessor = m_temperatureGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> gridSampler(accessor, m_temperatureGrid->transform());

            Point3f localP = m_worldToLocal(pWorld);
            openvdb::Vec3d vdbP(localP.x(), localP.y(), localP.z());

            openvdb::Vec3d indexP = m_temperatureGrid->transform().worldToIndex(vdbP);
            float tempRaw = gridSampler.isSample(indexP);

            if (tempRaw <= 0.0f) return Color3f(0.0f);

            float kelvin = (tempRaw * m_temperatureScale) + m_temperatureOffset;

            Color3f emitColor = blackbody(kelvin);

            return emitColor * (tempRaw * m_emissionScale);
        }

        Color3f sample(const Ray& ray, Sampler& sampler, MemoryArena& arena, MediumInteraction& mi) const override {
            if (m_majorant <= 0.0f) return Color3f(1.0f);

            float tMax = ray.tMax;
            float t = ray.tMin;

            auto accessor = m_densityGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> gridSampler(accessor, m_densityGrid->transform());

            Point3f localO = m_worldToLocal(ray.o);
            Point3f localTarget = m_worldToLocal(ray.o + ray.d);

            openvdb::Vec3d vdbO(localO.x(), localO.y(), localO.z());
            openvdb::Vec3d vdbTarget(localTarget.x(), localTarget.y(), localTarget.z());

            const openvdb::math::Transform& gridTrans = m_densityGrid->transform();
            openvdb::Vec3d indexO = gridTrans.worldToIndex(vdbO);
            openvdb::Vec3d indexD = gridTrans.worldToIndex(vdbTarget) - indexO;

            while (true) {
                t += -std::log(1.0f - sampler.next1D()) * m_invMajorant;
                if (t >= tMax) return Color3f(1.0f);

                openvdb::Vec3d pIndex = indexO + indexD * (double)t;
                float localDensity = gridSampler.isSample(pIndex) * m_densityScale;

                if (localDensity > 0.0f) {
                    int channel = std::min(static_cast<int>(sampler.next1D() * 3.0f), 2);

                    float prob_real_collision = localDensity * m_base_sigma_t[channel] * m_invMajorant;

                    if (sampler.next1D() < prob_real_collision) {
                        mi.p = ray.o + ray.d * t;
                        mi.wo = -ray.d;
                        mi.medium = this;
                        mi.phase = arena.create<HenyeyGreensteinPhaseFunction>(m_g);

                        Color3f local_sigma_t = m_base_sigma_t * localDensity;
                        Color3f local_sigma_s = m_base_sigma_s * localDensity;
                        return local_sigma_s / local_sigma_t[channel];
                    }
                }
            }
        }

        virtual float getDensity(const Point3f& p) const override {
            auto accessor = m_densityGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> gridSampler(accessor, m_densityGrid->transform());

            Point3f pLocal = m_worldToLocal(p);
            openvdb::Vec3d pVdb(pLocal.x(), pLocal.y(), pLocal.z());

            return gridSampler.wsSample(pVdb) * m_densityScale;
        }

        std::string toString() const override {
            std::string emissionInfo = "none";

            if (m_temperatureGrid) {
                emissionInfo = std::format(
                    "[\n"
                    "    grid = {},\n"
                    "    scale = {},\n"
                    "    offset = {},\n"
                    "    intensity = {}\n"
                    "  ]",
                    m_temperatureGridName,
                    m_temperatureScale,
                    m_temperatureOffset,
                    m_emissionScale
                );
            }

            return std::format(
                "HeterogeneousMedium[\n"
                "  majorant = {}\n"
                "  base absorption = {}\n"
                "  base scattering = {}\n"
                "  base transmittance = {}\n"
                "  anisotropy (g) = {}\n"
                "  emission = {}\n"
                "]",
                m_majorant,
                m_base_sigma_a.toString(),
                m_base_sigma_s.toString(),
                m_base_sigma_t.toString(),
                m_g,
                indent(emissionInfo, 2)
            );
        }

    private:
        Color3f m_base_sigma_a;
        Color3f m_base_sigma_s;
        Color3f m_base_sigma_t;
        float m_g;

        Transform m_worldToLocal;
        
        float m_majorant;
        float m_invMajorant;

        // Density grid
        openvdb::FloatGrid::Ptr m_densityGrid;
        float m_densityScale;

        // Temperature grid
        std::string m_temperatureGridName;
        openvdb::FloatGrid::Ptr m_temperatureGrid;
        float m_temperatureScale;
        float m_temperatureOffset;
        float m_emissionScale;

        // Helper to convert Kelvin to RGB
        Color3f blackbody(float temp) const {
            if (temp <= 1000.0f) return Color3f(0.0f);

            temp /= 100.0f;
            float r, g, b;

            // Red
            if (temp <= 66.0f) r = 255.0f;
            else r = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);

            // Green
            if (temp <= 66.0f) g = 99.4708025861f * std::log(temp) - 161.1195681661f;
            else g = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);

            // Blue
            if (temp >= 66.0f) b = 255.0f;
            else if (temp <= 19.0f) b = 0.0f;
            else b = 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;

            return Color3f(
                std::clamp(r / 255.0f, 0.0f, 1.0f),
                std::clamp(g / 255.0f, 0.0f, 1.0f),
                std::clamp(b / 255.0f, 0.0f, 1.0f)
            );
        }
    };

    GND_REGISTER_CLASS(HeterogeneousMedium, "heterogeneous");
}
