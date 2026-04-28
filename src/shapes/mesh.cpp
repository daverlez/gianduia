#include "gianduia/shapes/mesh.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/fileResolver.h"
#include "gianduia/core/bvhTraversal.h"

#include <iostream>
#include <filesystem>

#include <tiny_obj_loader.h>

namespace gnd {

    Mesh::Mesh(const PropertyList& props) : Shape(props) {
        m_filename = props.getString("filename", "");
    }

    void Mesh::activate() {
        if (m_filename.empty())
            throw std::runtime_error("Mesh: filename missing!");

        std::string fullPath = FileResolver::resolveStr(m_filename);
        std::cout << "Mesh: Loading from resolved path: " << fullPath << std::endl;
        loadOBJ(fullPath);

        if (m_indices.empty())
            throw std::runtime_error("Mesh: no geometry loaded!");

        // ---- BVH construction
        size_t triangleCount = m_indices.size() / 3;
        std::vector<BVHBuildInfo> buildData;
        buildData.reserve(triangleCount);

        for (size_t i = 0; i < triangleCount; ++i) {
            uint32_t i0 = m_indices[i * 3 + 0];
            uint32_t i1 = m_indices[i * 3 + 1];
            uint32_t i2 = m_indices[i * 3 + 2];

            const Point3f& p0 = m_positions[i0];
            const Point3f& p1 = m_positions[i1];
            const Point3f& p2 = m_positions[i2];

            // Triangle bounds
            Bounds3f b;
            b = Union(b, p0);
            b = Union(b, p1);
            b = Union(b, p2);

            buildData.push_back({ b, b.centroid(), (int)i });
        }

        BVHBuildResult result = BVHBuilder::build(buildData);
        m_nodes = std::move(result.nodes);

        if (!m_nodes.empty())
            m_bounds = m_nodes[0].bounds;

        // ---- SoA for SIMD
        m_trianglePacks.clear();

        for (auto& node : m_nodes) {
            if (node.nPrimitives > 0) {
                int origOffset = node.primitivesOffset;
                int origCount = node.nPrimitives;

                int packOffset = (int)m_trianglePacks.size();
                int numPacks = (origCount + 3) / 4;

                for (int p = 0; p < numPacks; ++p) {
                    TrianglePack4 pack;

                    // Initialize to degenerate triangles with zero area
                    for (int l = 0; l < 4; ++l) {
                        pack.v0x[l] = pack.v0y[l] = pack.v0z[l] = 0.0f;
                        pack.v1x[l] = pack.v1y[l] = pack.v1z[l] = 0.0f;
                        pack.v2x[l] = pack.v2y[l] = pack.v2z[l] = 0.0f;
                        pack.primIndex[l] = (uint32_t)-1;
                    }

                    // Filling the lanes with the triangle
                    for (int l = 0; l < 4; ++l) {
                        int triIdx = p * 4 + l;
                        if (triIdx < origCount) {
                            int originalTri = result.orderedIndices[origOffset + triIdx];
                            pack.primIndex[l] = originalTri;

                            uint32_t i0 = m_indices[originalTri * 3 + 0];
                            uint32_t i1 = m_indices[originalTri * 3 + 1];
                            uint32_t i2 = m_indices[originalTri * 3 + 2];

                            const Point3f& p0 = m_positions[i0];
                            const Point3f& p1 = m_positions[i1];
                            const Point3f& p2 = m_positions[i2];

                            pack.v0x[l] = p0.x(); pack.v0y[l] = p0.y(); pack.v0z[l] = p0.z();
                            pack.v1x[l] = p1.x(); pack.v1y[l] = p1.y(); pack.v1z[l] = p1.z();
                            pack.v2x[l] = p2.x(); pack.v2y[l] = p2.y(); pack.v2z[l] = p2.z();
                        }
                    }
                    m_trianglePacks.push_back(pack);
                }

                // BVH update: offset is now related to SoA list, and nPrimitives is the number of packets
                node.primitivesOffset = packOffset;
                node.nPrimitives = numPacks;
            }
        }


        // ---- Tree Collapsing (binary BVH to BVH4)
        if (!m_nodes.empty()) {
            m_nodes4 = BVHBuilder::buildWide(m_nodes);
            m_nodes.clear();
            m_nodes.shrink_to_fit();
        }

        std::cout << "Mesh: Done! Built a BVH with " << m_nodes4.size()
                  << " wide nodes, occupying " << m_nodes4.size() * sizeof(BVHNode4) << " bytes, "
                  "packed into " << m_trianglePacks.size() << " SIMD TrianglePacks." << std::endl;

        // ---- Computing CDF
        m_cdf.resize(triangleCount + 1, 0.0f);
        m_totalArea = 0.0f;

        for (size_t i = 0; i < triangleCount; ++i) {
            uint32_t i0 = m_indices[i * 3 + 0];
            uint32_t i1 = m_indices[i * 3 + 1];
            uint32_t i2 = m_indices[i * 3 + 2];

            const Point3f& p0 = m_positions[i0];
            const Point3f& p1 = m_positions[i1];
            const Point3f& p2 = m_positions[i2];

            Vector3f e1 = p1 - p0;
            Vector3f e2 = p2 - p0;
            float area = 0.5f * Cross(e1, e2).length();

            m_totalArea += area;
            m_cdf[i + 1] = m_totalArea;
        }

        if (m_totalArea > 0.0f) {
            for (size_t i = 1; i <= triangleCount; ++i) {
                m_cdf[i] /= m_totalArea;
            }
        }
    }

    void Mesh::loadOBJ(const std::string& filename) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        std::filesystem::path filepath(filename);
        std::string baseDir = filepath.parent_path().string() + "/";

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), baseDir.c_str());

        if (!warn.empty()) std::cout << "OBJ Warning: " << warn << std::endl;
        if (!err.empty()) std::cerr << "OBJ Error: " << err << std::endl;
        if (!ret) throw std::runtime_error("Failed to load OBJ: " + filename);

        bool hasNormals = !attrib.normals.empty();
        std::vector<Vector3f> generatedNormals;

        if (!hasNormals) {
            std::cout << "Mesh: Normals missing in OBJ, generating smooth normals..." << std::endl;
            generatedNormals.resize(attrib.vertices.size() / 3, Vector3f(0.0f, 0.0f, 0.0f));

            for (const auto& shape : shapes) {
                size_t index_offset = 0;
                for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
                    tinyobj::index_t idx0 = shape.mesh.indices[index_offset + 0];
                    tinyobj::index_t idx1 = shape.mesh.indices[index_offset + 1];
                    tinyobj::index_t idx2 = shape.mesh.indices[index_offset + 2];

                    Point3f p0(attrib.vertices[3 * idx0.vertex_index + 0], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]);
                    Point3f p1(attrib.vertices[3 * idx1.vertex_index + 0], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]);
                    Point3f p2(attrib.vertices[3 * idx2.vertex_index + 0], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]);

                    Vector3f weightedNormal = Cross(p1 - p0, p2 - p0);

                    generatedNormals[idx0.vertex_index] = generatedNormals[idx0.vertex_index] + weightedNormal;
                    generatedNormals[idx1.vertex_index] = generatedNormals[idx1.vertex_index] + weightedNormal;
                    generatedNormals[idx2.vertex_index] = generatedNormals[idx2.vertex_index] + weightedNormal;

                    index_offset += 3;
                }
            }

            for (auto& n : generatedNormals) {
                float len = n.length();
                if (len > 0.0f) n = n / len;
            }
        }

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Point3f pos(
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                );
                m_positions.push_back(pos);

                if (hasNormals && index.normal_index >= 0) {
                    Normal3f norm(
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    );
                    m_normals.push_back(Normalize(norm));
                } else {
                    Vector3f genN = generatedNormals[index.vertex_index];
                    m_normals.push_back(Normal3f(genN.x(), genN.y(), genN.z()));
                }

                if (index.texcoord_index >= 0) {
                    Point2f uv(
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1]
                    );
                    m_uvs.push_back(uv);
                }

                m_indices.push_back((uint32_t)m_indices.size());
            }
        }
    }

    bool Mesh::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const {
        hn::FixedTag<float, 4> d;
        using V = hn::Vec<decltype(d)>;
        using M = hn::Mask<decltype(d)>;

        V r_ox = hn::Set(d, ray.o.x());
        V r_oy = hn::Set(d, ray.o.y());
        V r_oz = hn::Set(d, ray.o.z());

        V r_dx = hn::Set(d, ray.d.x());
        V r_dy = hn::Set(d, ray.d.y());
        V r_dz = hn::Set(d, ray.d.z());

        auto crossX = [](V ax, V ay, V az, V bx, V by, V bz) { return hn::Sub(hn::Mul(ay, bz), hn::Mul(az, by)); };
        auto crossY = [](V ax, V ay, V az, V bx, V by, V bz) { return hn::Sub(hn::Mul(az, bx), hn::Mul(ax, bz)); };
        auto crossZ = [](V ax, V ay, V az, V bx, V by, V bz) { return hn::Sub(hn::Mul(ax, by), hn::Mul(ay, bx)); };
        auto dot = [](V ax, V ay, V az, V bx, V by, V bz) {
            return hn::Add(hn::Add(hn::Mul(ax, bx), hn::Mul(ay, by)), hn::Mul(az, bz));
        };

        auto leafIntersector = [&](int offset, int count) -> bool {
            bool hitLocal = false;

            V t_max_simd = hn::Set(d, ray.tMax);

            for (int p = 0; p < count; ++p) {
                const TrianglePack4& pack = m_trianglePacks[offset + p];

                V v0x = hn::Load(d, pack.v0x); V v0y = hn::Load(d, pack.v0y); V v0z = hn::Load(d, pack.v0z);
                V v1x = hn::Load(d, pack.v1x); V v1y = hn::Load(d, pack.v1y); V v1z = hn::Load(d, pack.v1z);
                V v2x = hn::Load(d, pack.v2x); V v2y = hn::Load(d, pack.v2y); V v2z = hn::Load(d, pack.v2z);

                V e1x = hn::Sub(v1x, v0x); V e1y = hn::Sub(v1y, v0y); V e1z = hn::Sub(v1z, v0z);
                V e2x = hn::Sub(v2x, v0x); V e2y = hn::Sub(v2y, v0y); V e2z = hn::Sub(v2z, v0z);

                V pvecx = crossX(r_dx, r_dy, r_dz, e2x, e2y, e2z);
                V pvecy = crossY(r_dx, r_dy, r_dz, e2x, e2y, e2z);
                V pvecz = crossZ(r_dx, r_dy, r_dz, e2x, e2y, e2z);

                V det = dot(e1x, e1y, e1z, pvecx, pvecy, pvecz);
                V eps = hn::Set(d, 1e-8f);
                M validTri = hn::Gt(hn::Abs(det), eps);

                V invDet = hn::Div(hn::Set(d, 1.0f), det);
                V tvecx = hn::Sub(r_ox, v0x); V tvecy = hn::Sub(r_oy, v0y); V tvecz = hn::Sub(r_oz, v0z);

                V u = hn::Mul(dot(tvecx, tvecy, tvecz, pvecx, pvecy, pvecz), invDet);
                validTri = hn::And(validTri, hn::Ge(u, hn::Set(d, 0.0f)));
                validTri = hn::And(validTri, hn::Le(u, hn::Set(d, 1.0f)));

                V qvecx = crossX(tvecx, tvecy, tvecz, e1x, e1y, e1z);
                V qvecy = crossY(tvecx, tvecy, tvecz, e1x, e1y, e1z);
                V qvecz = crossZ(tvecx, tvecy, tvecz, e1x, e1y, e1z);

                V v = hn::Mul(dot(r_dx, r_dy, r_dz, qvecx, qvecy, qvecz), invDet);
                validTri = hn::And(validTri, hn::Ge(v, hn::Set(d, 0.0f)));
                validTri = hn::And(validTri, hn::Le(hn::Add(u, v), hn::Set(d, 1.0f)));

                V t = hn::Mul(dot(e2x, e2y, e2z, qvecx, qvecy, qvecz), invDet);
                validTri = hn::And(validTri, hn::Gt(t, hn::Set(d, ray.tMin)));
                validTri = hn::And(validTri, hn::Lt(t, t_max_simd));

                if (!hn::AllFalse(d, validTri)) {
                    alignas(16) float t_arr[4];
                    alignas(16) float u_arr[4];
                    alignas(16) float v_arr[4];

                    hn::Store(t, d, t_arr);
                    hn::Store(u, d, u_arr);
                    hn::Store(v, d, v_arr);

                    uint8_t mask_tri[8] = {0};
                    hn::StoreMaskBits(d, validTri, mask_tri);

                    for (int l = 0; l < 4; ++l) {
                        if ((mask_tri[0] & (1 << l)) != 0) {
                            if (t_arr[l] < ray.tMax) {
                                if (predicate) return true;

                                ray.tMax = t_arr[l];
                                t_max_simd = hn::Set(d, ray.tMax);

                                isect.t = t_arr[l];
                                isect.uv = Point2f(u_arr[l], v_arr[l]);
                                isect.primIndex = pack.primIndex[l];
                                hitLocal = true;
                            }
                        }
                    }
                }
            }
            return hitLocal;
        };

        return traverseBVH4(m_nodes4, ray, predicate, leafIntersector);
    }

    void Mesh::fillInteraction(const Ray& ray, SurfaceInteraction& isect) const {
        uint32_t i0 = m_indices[isect.primIndex * 3 + 0];
        uint32_t i1 = m_indices[isect.primIndex * 3 + 1];
        uint32_t i2 = m_indices[isect.primIndex * 3 + 2];

        const Point3f& p0 = m_positions[i0];
        const Point3f& p1 = m_positions[i1];
        const Point3f& p2 = m_positions[i2];

        float u = isect.uv.x();
        float v = isect.uv.y();
        float w = 1.0f - u - v;

        isect.p = p0 * w + p1 * u + p2 * v;

        if (!m_normals.empty()) {
            Normal3f n0 = m_normals[i0];
            Normal3f n1 = m_normals[i1];
            Normal3f n2 = m_normals[i2];

            isect.n = Normalize(n0 * w + n1 * u + n2 * v);
        } else {
            Vector3f e1 = m_positions[i1] - m_positions[i0];
            Vector3f e2 = m_positions[i2] - m_positions[i0];
            isect.n = Normal3f(Normalize(Cross(e1, e2)));
        }

        if (!m_uvs.empty()) {
            Point2f uv0 = m_uvs[i0];
            Point2f uv1 = m_uvs[i1];
            Point2f uv2 = m_uvs[i2];
            isect.uv = uv0 * w + uv1 * u + uv2 * v;

            Vector3f dp1 = p1 - p0;
            Vector3f dp2 = p2 - p0;
            Vector2f duv1 = uv1 - uv0;
            Vector2f duv2 = uv2 - uv0;

            float det = duv1.x() * duv2.y() - duv1.y() * duv2.x();
            Vector3f dpdu;

            if (std::abs(det) < 1e-8f) {
                if (std::abs(isect.n.x()) > std::abs(isect.n.y()))
                    dpdu = Vector3f(-isect.n.z(), 0.0f, isect.n.x()) / std::sqrt(isect.n.x() * isect.n.x() + isect.n.z() * isect.n.z());
                else
                    dpdu = Vector3f(0.0f, isect.n.z(), -isect.n.y()) / std::sqrt(isect.n.y() * isect.n.y() + isect.n.z() * isect.n.z());
            } else {
                float invDet = 1.0f / det;
                dpdu = (dp1 * duv2.y() - dp2 * duv1.y()) * invDet;
            }

            isect.dpdu = Normalize(dpdu - isect.n * Dot(isect.n, dpdu));

        } else {
            Vector3f dpdu;
            if (std::abs(isect.n.x()) > std::abs(isect.n.y()))
                dpdu = Vector3f(-isect.n.z(), 0.0f, isect.n.x()) / std::sqrt(isect.n.x() * isect.n.x() + isect.n.z() * isect.n.z());
            else
                dpdu = Vector3f(0.0f, isect.n.z(), -isect.n.y()) / std::sqrt(isect.n.y() * isect.n.y() + isect.n.z() * isect.n.z());

            isect.dpdu = Normalize(dpdu);
        }
    }

    Bounds3f Mesh::getBounds() const {
        return m_bounds;
    }

    float Mesh::sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& isect) const {
        if (m_totalArea == 0.0f) return 0.0f;

        auto it = std::lower_bound(m_cdf.begin(), m_cdf.end(), sample.x());
        size_t triIndex = std::distance(m_cdf.begin(), it);
        triIndex = std::clamp(triIndex, (size_t)1, m_cdf.size() - 1) - 1;

        float pdfTri = m_cdf[triIndex + 1] - m_cdf[triIndex];
        float remapped_x = (sample.x() - m_cdf[triIndex]) / pdfTri;
        remapped_x = std::clamp(remapped_x, 0.0f, 1.0f);

        float sqrtXi = std::sqrt(remapped_x);
        float u = 1.0f - sqrtXi;
        float v = sample.y() * sqrtXi;
        float w = 1.0f - u - v;

        uint32_t i0 = m_indices[triIndex * 3 + 0];
        uint32_t i1 = m_indices[triIndex * 3 + 1];
        uint32_t i2 = m_indices[triIndex * 3 + 2];

        isect.p = m_positions[i0] * w + m_positions[i1] * u + m_positions[i2] * v;

        if (!m_normals.empty()) {
            Normal3f n0 = m_normals[i0];
            Normal3f n1 = m_normals[i1];
            Normal3f n2 = m_normals[i2];
            isect.n = Normalize(n0 * w + n1 * u + n2 * v);
        } else {
            Vector3f e1 = m_positions[i1] - m_positions[i0];
            Vector3f e2 = m_positions[i2] - m_positions[i0];
            isect.n = Normal3f(Normalize(Cross(e1, e2)));
        }

        if (!m_uvs.empty()) {
            isect.uv = m_uvs[i0] * w + m_uvs[i1] * u + m_uvs[i2] * v;
        }

        return 1.0f / m_totalArea;
    }

    float Mesh::pdfSurface(const Point3f& ref, const SurfaceInteraction& isect) const {
        return m_totalArea > 0.0f ? (1.0f / m_totalArea) : 0.0f;
    }

    std::string Mesh::toString() const {
        return std::format("Mesh[file={}, tris={}]", m_filename, m_indices.size()/3);
    }

    GND_REGISTER_CLASS(Mesh, "mesh");
}
