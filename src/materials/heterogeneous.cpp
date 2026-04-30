#include <gianduia/materials/medium.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/sampler.h>
#include <gianduia/math/color.h>
#include <gianduia/math/bounds.h>
#include <gianduia/core/fileResolver.h>

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/Statistics.h>

#include <cmath>
#include <algorithm>
#include <vector>

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

            std::string absPath = (FileResolver::resolve(filename)).string();

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

            // Macro grid setup
            openvdb::CoordBBox bbox = m_densityGrid->evalActiveVoxelBoundingBox();

            m_gridMin = Point3f(bbox.min().x(), bbox.min().y(), bbox.min().z());
            m_gridMax = Point3f(bbox.max().x() + 1.0f, bbox.max().y() + 1.0f, bbox.max().z() + 1.0f);

            float extentX = m_gridMax.x() - m_gridMin.x();
            float extentY = m_gridMax.y() - m_gridMin.y();
            float extentZ = m_gridMax.z() - m_gridMin.z();

            int targetRes = props.getInteger("macroRes", 32);
            float maxExtent = std::max({extentX, extentY, extentZ});

            m_resX = std::max(1, static_cast<int>(std::round(targetRes * (extentX / maxExtent))));
            m_resY = std::max(1, static_cast<int>(std::round(targetRes * (extentY / maxExtent))));
            m_resZ = std::max(1, static_cast<int>(std::round(targetRes * (extentZ / maxExtent))));

            m_cellSizeX = extentX / m_resX;
            m_cellSizeY = extentY / m_resY;
            m_cellSizeZ = extentZ / m_resZ;

            m_macroMajorants.assign(m_resX * m_resY * m_resZ, 0.0f);

            float max_color_t = std::max({m_base_sigma_t.r(), m_base_sigma_t.g(), m_base_sigma_t.b()});

            for (auto iter = m_densityGrid->tree().cbeginValueOn(); iter; ++iter) {
                openvdb::Coord coord = iter.getCoord();
                float density = *iter * m_densityScale;

                if (density <= 0.0f || !std::isfinite(density)) continue;

                int cx = std::clamp(static_cast<int>((coord.x() - m_gridMin.x()) / m_cellSizeX), 0, m_resX - 1);
                int cy = std::clamp(static_cast<int>((coord.y() - m_gridMin.y()) / m_cellSizeY), 0, m_resY - 1);
                int cz = std::clamp(static_cast<int>((coord.z() - m_gridMin.z()) / m_cellSizeZ), 0, m_resZ - 1);

                int flatIdx = cz * (m_resX * m_resY) + cy * m_resX + cx;
                float local_maj = density * max_color_t;

                if (local_maj > m_macroMajorants[flatIdx])
                    m_macroMajorants[flatIdx] = local_maj;
            }
        }

        Color3f Tr(const Ray& ray, Sampler& sampler) const override {
            float tMax = ray.tMax;
            float t = ray.tMin;
            Color3f tr(1.0f);

            auto accessor = m_densityGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> gridSampler(accessor, m_densityGrid->transform());

            Point3f localO = m_worldToLocal(ray.o);
            Point3f localTarget = m_worldToLocal(ray.o + ray.d);

            openvdb::Vec3d vdbO(localO.x(), localO.y(), localO.z());
            openvdb::Vec3d vdbTarget(localTarget.x(), localTarget.y(), localTarget.z());

            const openvdb::math::Transform& gridTrans = m_densityGrid->transform();
            openvdb::Vec3d indexO = gridTrans.worldToIndex(vdbO);
            openvdb::Vec3d indexD = gridTrans.worldToIndex(vdbTarget) - indexO;

            Ray indexRay(Point3f(indexO.x(), indexO.y(), indexO.z()),
                         Vector3f(indexD.x(), indexD.y(), indexD.z()));
            indexRay.tMin = t;
            indexRay.tMax = tMax;

            Bounds3f gridBounds(m_gridMin, m_gridMax);
            float hit_t0, hit_t1;
            if (!gridBounds.rayIntersect(indexRay, &hit_t0, &hit_t1)) {
                return tr;
            }

            t = std::max(t, hit_t0);
            tMax = std::min(tMax, hit_t1);

            Point3f pEntry = indexRay.o + indexRay.d * t;
            int cx = std::clamp(static_cast<int>(std::floor((pEntry.x() - m_gridMin.x()) / m_cellSizeX)), 0, m_resX - 1);
            int cy = std::clamp(static_cast<int>(std::floor((pEntry.y() - m_gridMin.y()) / m_cellSizeY)), 0, m_resY - 1);
            int cz = std::clamp(static_cast<int>(std::floor((pEntry.z() - m_gridMin.z()) / m_cellSizeZ)), 0, m_resZ - 1);

            int stepX = (indexRay.d.x() > 0.0f) ? 1 : -1;
            int stepY = (indexRay.d.y() > 0.0f) ? 1 : -1;
            int stepZ = (indexRay.d.z() > 0.0f) ? 1 : -1;

            float tDeltaX = (indexRay.d.x() != 0.0f) ? std::abs(m_cellSizeX / indexRay.d.x()) : std::numeric_limits<float>::max();
            float tDeltaY = (indexRay.d.y() != 0.0f) ? std::abs(m_cellSizeY / indexRay.d.y()) : std::numeric_limits<float>::max();
            float tDeltaZ = (indexRay.d.z() != 0.0f) ? std::abs(m_cellSizeZ / indexRay.d.z()) : std::numeric_limits<float>::max();

            float nextBoundX = m_gridMin.x() + (cx + (stepX > 0 ? 1 : 0)) * m_cellSizeX;
            float nextBoundY = m_gridMin.y() + (cy + (stepY > 0 ? 1 : 0)) * m_cellSizeY;
            float nextBoundZ = m_gridMin.z() + (cz + (stepZ > 0 ? 1 : 0)) * m_cellSizeZ;

            float tMaxX = (indexRay.d.x() != 0.0f) ? (nextBoundX - indexRay.o.x()) / indexRay.d.x() : std::numeric_limits<float>::max();
            float tMaxY = (indexRay.d.y() != 0.0f) ? (nextBoundY - indexRay.o.y()) / indexRay.d.y() : std::numeric_limits<float>::max();
            float tMaxZ = (indexRay.d.z() != 0.0f) ? (nextBoundZ - indexRay.o.z()) / indexRay.d.z() : std::numeric_limits<float>::max();

            if (tMaxX <= t) tMaxX += tDeltaX;
            if (tMaxY <= t) tMaxY += tDeltaY;
            if (tMaxZ <= t) tMaxZ += tDeltaZ;

            while (t < tMax) {
                if (cx < 0 || cx >= m_resX || cy < 0 || cy >= m_resY || cz < 0 || cz >= m_resZ) break;

                int flatIdx = cz * (m_resX * m_resY) + cy * m_resX + cx;
                float local_maj = m_macroMajorants[flatIdx];

                float tCellExit = std::min({tMaxX, tMaxY, tMaxZ, tMax});

                if (local_maj > 0.0f) {
                    float invMaj = 1.0f / local_maj;

                    while (t < tCellExit) {
                        float step = -std::log(1.0f - sampler.next1D()) * invMaj;
                        if (step <= 0.0f) step = 1e-5f;

                        if (t + step >= tCellExit) {
                            break;
                        }

                        t += step;

                        openvdb::Vec3d pVdb(indexRay.o.x() + indexRay.d.x() * t,
                                            indexRay.o.y() + indexRay.d.y() * t,
                                            indexRay.o.z() + indexRay.d.z() * t);

                        float localDensity = gridSampler.isSample(pVdb) * m_densityScale;
                        if (localDensity < 1e-5f) localDensity = 0.0f;

                        if (localDensity > 0.0f) {
                            Color3f local_sigma_t = m_base_sigma_t * localDensity;
                            tr *= (Color3f(1.0f) - local_sigma_t * invMaj);

                            float maxTr = std::max({tr.r(), tr.g(), tr.b()});
                            if (maxTr < 1e-4f) return Color3f(0.0f);

                            if (maxTr < 0.1f) {
                                float q = std::max(0.05f, 1.0f - maxTr);
                                if (sampler.next1D() < q) return Color3f(0.0f);
                                tr /= (1.0f - q);
                            }
                        }
                    }
                }

                t = tCellExit;
                if (t >= tMax) break;

                if (tMaxX < tMaxY) {
                    if (tMaxX < tMaxZ) {
                        tMaxX += tDeltaX;
                        cx += stepX;
                    } else {
                        tMaxZ += tDeltaZ;
                        cz += stepZ;
                    }
                } else {
                    if (tMaxY < tMaxZ) {
                        tMaxY += tDeltaY;
                        cy += stepY;
                    } else {
                        tMaxZ += tDeltaZ;
                        cz += stepZ;
                    }
                }
            }
            return tr;
        }

        virtual Color3f Le(const Point3f& pWorld) const override {
            if (!m_temperatureGrid || m_emissionScale <= 0.0f) return Color3f(0.0f);

            auto accessor = m_temperatureGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> gridSampler(accessor, m_temperatureGrid->transform());

            Point3f localP = m_worldToLocal(pWorld);
            openvdb::Vec3d vdbP(localP.x(), localP.y(), localP.z());

            openvdb::Vec3d indexP = m_temperatureGrid->transform().worldToIndex(vdbP);
            float tempRaw = gridSampler.isSample(indexP);

            if (tempRaw < 1e-5f) return Color3f(0.0f);

            float kelvin = (tempRaw * m_temperatureScale) + m_temperatureOffset;
            Color3f emitColor = blackbody(kelvin);

            return emitColor * (tempRaw * m_emissionScale);
        }

        Color3f sample(const Ray& ray, Sampler& sampler, MemoryArena& arena, MediumInteraction& mi) const override {
            float tMax = ray.tMax;
            float t = ray.tMin;

            auto accessor = m_densityGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> gridSampler(accessor, m_densityGrid->transform());

            Point3f localO = m_worldToLocal(ray.o);
            Point3f localTarget = m_worldToLocal(ray.o + ray.d);

            openvdb::Vec3d vdbO(localO.x(), localO.y(), localO.z());
            openvdb::Vec3d vdbTarget(localTarget.x(), localTarget.y(), localTarget.z());

            const openvdb::math::Transform& gridTrans = m_densityGrid->transform();
            openvdb::Vec3d indexO = gridTrans.worldToIndex(vdbO);
            openvdb::Vec3d indexD = gridTrans.worldToIndex(vdbTarget) - indexO;

            Ray indexRay(Point3f(indexO.x(), indexO.y(), indexO.z()),
                         Vector3f(indexD.x(), indexD.y(), indexD.z()));
            indexRay.tMin = t;
            indexRay.tMax = tMax;

            Bounds3f gridBounds(m_gridMin, m_gridMax);
            float hit_t0, hit_t1;
            if (!gridBounds.rayIntersect(indexRay, &hit_t0, &hit_t1)) {
                return Color3f(1.0f);
            }

            t = std::max(t, hit_t0);
            tMax = std::min(tMax, hit_t1);

            Point3f pEntry = indexRay.o + indexRay.d * t;
            int cx = std::clamp(static_cast<int>(std::floor((pEntry.x() - m_gridMin.x()) / m_cellSizeX)), 0, m_resX - 1);
            int cy = std::clamp(static_cast<int>(std::floor((pEntry.y() - m_gridMin.y()) / m_cellSizeY)), 0, m_resY - 1);
            int cz = std::clamp(static_cast<int>(std::floor((pEntry.z() - m_gridMin.z()) / m_cellSizeZ)), 0, m_resZ - 1);

            int stepX = (indexRay.d.x() > 0.0f) ? 1 : -1;
            int stepY = (indexRay.d.y() > 0.0f) ? 1 : -1;
            int stepZ = (indexRay.d.z() > 0.0f) ? 1 : -1;

            float tDeltaX = (indexRay.d.x() != 0.0f) ? std::abs(m_cellSizeX / indexRay.d.x()) : std::numeric_limits<float>::max();
            float tDeltaY = (indexRay.d.y() != 0.0f) ? std::abs(m_cellSizeY / indexRay.d.y()) : std::numeric_limits<float>::max();
            float tDeltaZ = (indexRay.d.z() != 0.0f) ? std::abs(m_cellSizeZ / indexRay.d.z()) : std::numeric_limits<float>::max();

            float nextBoundX = m_gridMin.x() + (cx + (stepX > 0 ? 1 : 0)) * m_cellSizeX;
            float nextBoundY = m_gridMin.y() + (cy + (stepY > 0 ? 1 : 0)) * m_cellSizeY;
            float nextBoundZ = m_gridMin.z() + (cz + (stepZ > 0 ? 1 : 0)) * m_cellSizeZ;

            float tMaxX = (indexRay.d.x() != 0.0f) ? (nextBoundX - indexRay.o.x()) / indexRay.d.x() : std::numeric_limits<float>::max();
            float tMaxY = (indexRay.d.y() != 0.0f) ? (nextBoundY - indexRay.o.y()) / indexRay.d.y() : std::numeric_limits<float>::max();
            float tMaxZ = (indexRay.d.z() != 0.0f) ? (nextBoundZ - indexRay.o.z()) / indexRay.d.z() : std::numeric_limits<float>::max();

            if (tMaxX <= t) tMaxX += tDeltaX;
            if (tMaxY <= t) tMaxY += tDeltaY;
            if (tMaxZ <= t) tMaxZ += tDeltaZ;

            while (t < tMax) {
                if (cx < 0 || cx >= m_resX || cy < 0 || cy >= m_resY || cz < 0 || cz >= m_resZ) break;

                int flatIdx = cz * (m_resX * m_resY) + cy * m_resX + cx;
                float local_maj = m_macroMajorants[flatIdx];

                float tCellExit = std::min({tMaxX, tMaxY, tMaxZ, tMax});

                if (local_maj > 0.0f) {
                    float invMaj = 1.0f / local_maj;

                    while (t < tCellExit) {
                        float step = -std::log(1.0f - sampler.next1D()) * invMaj;
                        if (step <= 0.0f) step = 1e-5f;

                        if (t + step >= tCellExit) {
                            break;
                        }

                        t += step;

                        openvdb::Vec3d pVdb(indexRay.o.x() + indexRay.d.x() * t,
                                            indexRay.o.y() + indexRay.d.y() * t,
                                            indexRay.o.z() + indexRay.d.z() * t);

                        float localDensity = gridSampler.isSample(pVdb) * m_densityScale;
                        if (localDensity < 1e-5f) localDensity = 0.0f;

                        if (localDensity > 0.0f) {
                            int channel = std::min(static_cast<int>(sampler.next1D() * 3.0f), 2);
                            float prob_real_collision = localDensity * m_base_sigma_t[channel] * invMaj;

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

                t = tCellExit;
                if (t >= tMax) break;

                if (tMaxX < tMaxY) {
                    if (tMaxX < tMaxZ) {
                        tMaxX += tDeltaX;
                        cx += stepX;
                    } else {
                        tMaxZ += tDeltaZ;
                        cz += stepZ;
                    }
                } else {
                    if (tMaxY < tMaxZ) {
                        tMaxY += tDeltaY;
                        cy += stepY;
                    } else {
                        tMaxZ += tDeltaZ;
                        cz += stepZ;
                    }
                }
            }
            return Color3f(1.0f);
        }

        virtual float getDensity(const Point3f& p) const override {
            auto accessor = m_densityGrid->getConstAccessor();
            openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> gridSampler(accessor, m_densityGrid->transform());

            Point3f pLocal = m_worldToLocal(p);
            openvdb::Vec3d pVdb(pLocal.x(), pLocal.y(), pLocal.z());

            float density = gridSampler.wsSample(pVdb) * m_densityScale;
            return density < 1e-5f ? 0.0f : density;
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
                    m_temperatureGridName, m_temperatureScale, m_temperatureOffset, m_emissionScale
                );
            }

            return std::format(
                "HeterogeneousMedium[\n"
                "  macro-grid = {}x{}x{}\n"
                "  base absorption = {}\n"
                "  base scattering = {}\n"
                "  base transmittance = {}\n"
                "  anisotropy (g) = {}\n"
                "  emission = {}\n"
                "]",
                m_resX, m_resY, m_resZ,
                m_base_sigma_a.toString(),
                m_base_sigma_s.toString(),
                m_base_sigma_t.toString(),
                m_g,
                indent(emissionInfo, 2)
            );
        }

    private:
        // Medium data
        Color3f m_base_sigma_a;
        Color3f m_base_sigma_s;
        Color3f m_base_sigma_t;
        float m_g;

        Transform m_worldToLocal;

        // Macro grid (local majorants)
        Point3f m_gridMin;
        Point3f m_gridMax;
        int m_resX, m_resY, m_resZ;
        float m_cellSizeX, m_cellSizeY, m_cellSizeZ;
        std::vector<float> m_macroMajorants;

        // Density grid
        openvdb::FloatGrid::Ptr m_densityGrid;
        float m_densityScale;

        // Temperature grid
        std::string m_temperatureGridName;
        openvdb::FloatGrid::Ptr m_temperatureGrid;
        float m_temperatureScale;
        float m_temperatureOffset;
        float m_emissionScale;

        Color3f blackbody(float temp) const {
            if (temp <= 1000.0f) return Color3f(0.0f);

            temp /= 100.0f;
            float r, g, b;

            if (temp <= 66.0f) r = 255.0f;
            else r = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);

            if (temp <= 66.0f) g = 99.4708025861f * std::log(temp) - 161.1195681661f;
            else g = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);

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