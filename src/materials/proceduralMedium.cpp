#include <gianduia/materials/medium.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/sampler.h>
#include <gianduia/math/color.h>
#include <gianduia/math/bounds.h>
#include <gianduia/math/perlin.h>

#include <cmath>
#include <algorithm>

namespace gnd {

    class ProceduralMedium : public Medium {
    public:
        ProceduralMedium(const PropertyList& props) {
            m_base_sigma_a = props.getColor("sigma_a", Color3f(0.1f));
            m_base_sigma_s = props.getColor("sigma_s", Color3f(0.1f));
            m_base_sigma_t = m_base_sigma_a + m_base_sigma_s;
            m_g = props.getFloat("g", 0.0f);

            m_worldToLocal = props.getTransform("toLocal", Transform());
            Point3f bMin = props.getPoint3("boundMin", Point3f(-1.0f));
            Point3f bMax = props.getPoint3("boundMax", Point3f(1.0f));
            m_localBounds = Bounds3f(bMin, bMax);

            m_noiseType = props.getInteger("noiseType", 1);
            m_densityScale = props.getFloat("densityScale", 1.0f);
            m_densityOffset = props.getFloat("densityOffset", 0.2f);
            m_noiseScale = props.getFloat("noiseScale", 1.0f);
            m_octaves = props.getInteger("octaves", 4);

            float max_color_t = std::max({m_base_sigma_t.r(), m_base_sigma_t.g(), m_base_sigma_t.b()});
            m_globalMajorant = max_color_t * m_densityScale;

            m_emissionColor = props.getColor("emissionColor", Color3f(1.0f, 1.0f, 1.0f));
            m_emissionScale = props.getFloat("emissionScale", 0.0f);
            m_emissionOffset = props.getFloat("emissionOffset", 0.4f);
            m_temperatureMin = props.getFloat("temperatureMin", 1000.0f);
            m_temperatureMax = props.getFloat("temperatureMax", 3500.0f);

            m_falloffStart = props.getFloat("falloffStart", 0.6f);
            m_useFalloff = m_falloffStart < 1.0f;
            m_localCenter = 0.5f * (m_localBounds.pMin + m_localBounds.pMax);
            m_localExtents = 0.5f * (m_localBounds.pMax - m_localBounds.pMin);
            std::string falloffType = props.getString("falloffType", "ellipsoid");
            if (falloffType == "box") m_falloffType = 1;
            else if (falloffType == "planar") m_falloffType = 2;
            else m_falloffType = 0;

            m_warpStrength = props.getFloat("warpStrength", 0.0f);
            m_warpScale = props.getFloat("warpScale", 1.0f);
        }

        Color3f Tr(const Ray& ray, Sampler& sampler) const override {
            if (m_globalMajorant <= 0.0f) return Color3f(1.0f);

            Ray localRay(m_worldToLocal(ray.o), m_worldToLocal(ray.d));
            float hit_t0, hit_t1;
            
            if (!m_localBounds.rayIntersect(localRay, &hit_t0, &hit_t1)) {
                return Color3f(1.0f);
            }

            float tMin = std::max(ray.tMin, hit_t0);
            float tMax = std::min(ray.tMax, hit_t1);
            float t = tMin;
            
            Color3f tr(1.0f);
            float invMaj = 1.0f / m_globalMajorant;

            while (t < tMax) {
                float step = -std::log(1.0f - sampler.next1D()) * invMaj;
                t += step;

                if (t >= tMax) break;

                Point3f pWorld = ray.o + ray.d * t;
                float localDensity = getDensity(pWorld);

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
            return tr;
        }

        virtual Color3f Le(const Point3f& pWorld) const override {
            Point3f pLocal = m_worldToLocal(pWorld);

            if (pLocal.x() < m_localBounds.pMin.x() || pLocal.x() > m_localBounds.pMax.x() ||
                pLocal.y() < m_localBounds.pMin.y() || pLocal.y() > m_localBounds.pMax.y() ||
                pLocal.z() < m_localBounds.pMin.z() || pLocal.z() > m_localBounds.pMax.z()) {
                return Color3f(0.0f);
            }

            Point3f pScaled = pLocal * m_noiseScale;
            pScaled = applyWarp(pScaled);
            float noiseValue = 0.0f;

            switch (m_noiseType) {
                case 0:
                    noiseValue = (Perlin::noise(pScaled) + 1.0f) * 0.5f;
                    break;
                case 1:
                    noiseValue = Perlin::fBm(pScaled, m_octaves);
                    break;
                case 2:
                    noiseValue = Perlin::turbulence(pScaled, m_octaves);
                    break;
                default:
                    noiseValue = Perlin::fBm(pScaled, m_octaves);
                    break;
            }

            float emissionStrength = std::clamp(noiseValue - m_emissionOffset, 0.0f, 1.0f);
            if (emissionStrength <= 0.0f) return Color3f(0.0f);

            float temp = Lerp(emissionStrength, m_temperatureMin, m_temperatureMax);
            Color3f fireColor = blackbody(temp) * m_emissionColor;

            return fireColor * (emissionStrength * m_emissionScale * evaluateFalloff(pLocal));
        }

        Color3f sample(const Ray& ray, Sampler& sampler, MemoryArena& arena, MediumInteraction& mi) const override {
            if (m_globalMajorant <= 0.0f) return Color3f(1.0f);

            Ray localRay(m_worldToLocal(ray.o), m_worldToLocal(ray.d));
            float hit_t0, hit_t1;
            
            if (!m_localBounds.rayIntersect(localRay, &hit_t0, &hit_t1)) {
                return Color3f(1.0f);
            }

            float tMin = std::max(ray.tMin, hit_t0);
            float tMax = std::min(ray.tMax, hit_t1);
            float t = tMin;
            
            float invMaj = 1.0f / m_globalMajorant;

            while (t < tMax) {
                float step = -std::log(1.0f - sampler.next1D()) * invMaj;
                t += step;

                if (t >= tMax) break;

                Point3f pWorld = ray.o + ray.d * t;
                float localDensity = getDensity(pWorld);

                if (localDensity > 0.0f) {
                    int channel = std::min(static_cast<int>(sampler.next1D() * 3.0f), 2);
                    float prob_real_collision = (localDensity * m_base_sigma_t[channel]) * invMaj;

                    if (sampler.next1D() < prob_real_collision) {
                        mi.p = pWorld;
                        mi.wo = -ray.d;
                        mi.medium = this;
                        mi.phase = arena.create<HenyeyGreensteinPhaseFunction>(m_g);

                        Color3f local_sigma_t = m_base_sigma_t * localDensity;
                        Color3f local_sigma_s = m_base_sigma_s * localDensity;

                        return local_sigma_s / local_sigma_t[channel];
                    }
                }
            }
            return Color3f(1.0f);
        }

        virtual float getDensity(const Point3f& pWorld) const override {
            Point3f pLocal = m_worldToLocal(pWorld);
            Point3f pScaled = pLocal * m_noiseScale;
            pScaled = applyWarp(pScaled);
            float noiseValue = 0.0f;

            switch (m_noiseType) {
                case 0:
                    noiseValue = (Perlin::noise(pScaled) + 1.0f) * 0.5f;
                    break;
                case 1:
                    noiseValue = Perlin::fBm(pScaled, m_octaves);
                    break;
                case 2:
                    noiseValue = Perlin::turbulence(pScaled, m_octaves);
                    break;
                default:
                    noiseValue = Perlin::fBm(pScaled, m_octaves);
                    break;
            }

            float densityMap = std::clamp(noiseValue - m_densityOffset, 0.0f, 1.0f);

            return densityMap * m_densityScale * evaluateFalloff(pLocal);
        }

        std::string toString() const override {
            std::string fTypeStr = "ellipsoid";
            if (m_falloffType == 1) fTypeStr = "box";
            else if (m_falloffType == 2) fTypeStr = "planar";

            return std::format(
                "ProceduralMedium[\n"
                "  bounds = {} -> {}\n"
                "  noise type = {}, octaves = {}, noise scale = {}\n"
                "  density scale = {}, offset = {}\n"
                "  emission scale = {}, offset = {}, temp = [{}K -> {}K], tint = {}\n"
                "  falloff enabled = {}, type = {}, start = {}\n"
                "  warp strength = {}, warp scale = {}\n"
                "  base absorption = {}\n"
                "  base scattering = {}\n"
                "  anisotropy (g) = {}\n"
                "]",
                m_localBounds.pMin.toString(), m_localBounds.pMax.toString(),
                m_noiseType, m_octaves, m_noiseScale,
                m_densityScale, m_densityOffset,
                m_emissionScale, m_emissionOffset, m_temperatureMin, m_temperatureMax, m_emissionColor.toString(),
                m_useFalloff ? "true" : "false", fTypeStr, m_falloffStart,
                m_warpStrength, m_warpScale,
                m_base_sigma_a.toString(),
                m_base_sigma_s.toString(),
                m_g
            );
        }

    private:
        Color3f m_base_sigma_a;
        Color3f m_base_sigma_s;
        Color3f m_base_sigma_t;
        float m_g;

        Transform m_worldToLocal;
        Bounds3f m_localBounds;
        float m_globalMajorant;

        // Density and noise pattern
        int m_noiseType; // 0: base noise; 1: fBm; 2: turbulence
        float m_densityScale;
        float m_densityOffset;
        float m_noiseScale;
        int m_octaves;

        // Emission data
        Color3f m_emissionColor;
        float m_emissionScale;
        float m_emissionOffset;
        float m_temperatureMin;
        float m_temperatureMax;

        // Falloff settings
        bool m_useFalloff;
        int m_falloffType;      // 0: ellipsoid; 1: box; 2: planar along z
        float m_falloffStart;
        Point3f m_localCenter;
        Vector3f m_localExtents;

        // Domain warping parameters
        float m_warpStrength;
        float m_warpScale;


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

        float evaluateFalloff(const Point3f& pLocal) const {
            if (!m_useFalloff) return 1.0f;

            float ex = std::max(m_localExtents.x(), Epsilon);
            float ey = std::max(m_localExtents.y(), Epsilon);
            float ez = std::max(m_localExtents.z(), Epsilon);

            float nx = std::abs((pLocal.x() - m_localCenter.x()) / ex);
            float ny = std::abs((pLocal.y() - m_localCenter.y()) / ey);
            float nz = std::abs((pLocal.z() - m_localCenter.z()) / ez);

            float r = 0.0f;

            if (m_falloffType == 0)
                r = std::sqrt(nx*nx + ny*ny + nz*nz);   // Ellipsoid (Euclidean distance)
            else if (m_falloffType == 1)
                r = std::max({nx, ny, nz});             // Box (Chebyshev distance)
            else if (m_falloffType == 2)
                r = nz;                                   // Planar along Z

            if (r < m_falloffStart) return 1.0f;
            if (r >= 1.0f) return 0.0f;

            float falloff = 1.0f - ((r - m_falloffStart) / (1.0f - m_falloffStart));
            return falloff * falloff * (3.0f - 2.0f * falloff);
        }

        Point3f applyWarp(const Point3f& pScaled) const {
            if (m_warpStrength <= 0.0f) return pScaled;

            Point3f warpP = pScaled * m_warpScale;

            // Arbitrary offsets along different coordinates to avoid shifting only along diagonals
            Vector3f offset(
                Perlin::noise(warpP),
                Perlin::noise(warpP + Point3f(13.2f, -4.3f, 7.8f)),
                Perlin::noise(warpP + Point3f(-21.1f, 18.4f, -15.5f))
            );

            return pScaled + (offset * m_warpStrength);
        }
    };

    GND_REGISTER_CLASS(ProceduralMedium, "procedural_medium");
}