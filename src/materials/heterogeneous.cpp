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
            m_densityScale = props.getFloat("densityScale", 1.0f);
            m_g = props.getFloat("g", 0.0f);
            m_worldToLocal = props.getTransform("toLocal", Transform());

            std::string filename = props.getString("filename", "");
            std::string gridName = props.getString("gridName", "density");

            std::string absPath = FileResolver::resolve(filename);

            openvdb::initialize();
            openvdb::io::File file(absPath);
            file.open();
            
            openvdb::GridBase::Ptr baseGrid;
            for (auto nameIter = file.beginName(); nameIter != file.endName(); ++nameIter) {
                if (nameIter.gridName() == gridName) {
                    baseGrid = file.readGrid(nameIter.gridName());
                    break;
                }
            }
            file.close();

            if (!baseGrid) throw std::runtime_error("HeterogeneousMedium: base VDB grid not found!");

            m_densityGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);

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
            return std::format(
                "HeterogeneousMedium[\n"
                "  majorant = {}\n"
                "  base absorption = {}\n"
                "  base scattering = {}\n"
                "  base transmittance = {}\n"
                "  anisotropy = {}\n"
                "]",
                m_majorant,
                m_base_sigma_a.toString(),
                m_base_sigma_s.toString(),
                m_base_sigma_t.toString(),
                m_g);
        }

    private:
        Color3f m_base_sigma_a;
        Color3f m_base_sigma_s;
        Color3f m_base_sigma_t;
        float m_densityScale;
        float m_g;
        Transform m_worldToLocal;
        
        float m_majorant;
        float m_invMajorant;
        
        openvdb::FloatGrid::Ptr m_densityGrid;
    };

    GND_REGISTER_CLASS(HeterogeneousMedium, "heterogeneous");
}
