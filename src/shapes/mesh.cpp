#include <gianduia/shapes/mesh.h>
#include <gianduia/core/factory.h>
#include <iostream>
#include <filesystem>

#include <tiny_obj_loader.h>

#include "gianduia/core/fileResolver.h"

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

        std::vector<uint32_t> orderedIndices;
        orderedIndices.reserve(m_indices.size());

        for (int originalTriIdx : result.orderedIndices) {
            orderedIndices.push_back(m_indices[originalTriIdx * 3 + 0]);
            orderedIndices.push_back(m_indices[originalTriIdx * 3 + 1]);
            orderedIndices.push_back(m_indices[originalTriIdx * 3 + 2]);
        }
        m_indices = std::move(orderedIndices);

        std::cout << "Mesh: Done! Built a BVH with " << m_nodes.size()
                    << " nodes occupying " << m_nodes.size() * sizeof(BVHNode) << " bytes." << std::endl;
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

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Point3f pos(
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                );
                m_positions.push_back(pos);

                if (index.normal_index >= 0) {
                    Normal3f norm(
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    );
                    m_normals.push_back(Normalize(norm));
                }

                if (index.texcoord_index >= 0) {
                    Point2f uv(
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip Y opzionale
                    );
                    m_uvs.push_back(uv);
                }

                m_indices.push_back((uint32_t)m_indices.size());
            }
        }

        // TODO: if no normals are defined, assign manually
    }

    bool Mesh::intersectTriangle(const Ray& ray, uint32_t triIndex, float& t, float& u, float& v) const {
        uint32_t i0 = m_indices[triIndex * 3 + 0];
        uint32_t i1 = m_indices[triIndex * 3 + 1];
        uint32_t i2 = m_indices[triIndex * 3 + 2];

        const Point3f& p0 = m_positions[i0];
        const Point3f& p1 = m_positions[i1];
        const Point3f& p2 = m_positions[i2];

        Vector3f edge1 = p1 - p0;
        Vector3f edge2 = p2 - p0;
        Vector3f pvec = Cross(ray.d, edge2);
        float det = Dot(edge1, pvec);

        if (std::abs(det) < 1e-8f) return false;

        float invDet = 1.0f / det;
        Vector3f tvec = ray.o - p0;
        u = Dot(tvec, pvec) * invDet;
        if (u < -Epsilon || u > 1.0f + Epsilon) return false;

        Vector3f qvec = Cross(tvec, edge1);
        v = Dot(ray.d, qvec) * invDet;
        if (v < -Epsilon || u + v > 1.0f + Epsilon) return false;

        t = Dot(edge2, qvec) * invDet;
        return true;
    }

    bool Mesh::rayIntersect(const Ray& ray, SurfaceInteraction& isect) const {
        if (m_nodes.empty()) return false;

        bool hitAny = false;
        int toVisitOffset = 0;
        int currentNodeIndex = 0;
        int nodesToVisit[64];

        while (true) {
            const BVHNode* node = &m_nodes[currentNodeIndex];

            if (node->bounds.rayIntersect(ray)) {

                if (node->nPrimitives > 0) {
                    // Leaf node
                    for (int i = 0; i < node->nPrimitives; ++i) {
                        int triIdx = node->primitivesOffset + i;

                        float t, u, v;
                        if (intersectTriangle(ray, triIdx, t, u, v)) {
                            if (t > ray.tMin && t < ray.tMax) {
                                ray.tMax = t; // Ray Shrinking

                                isect.t = t;
                                isect.uv = Point2f(u, v);
                                isect.primIndex = triIdx;
                                hitAny = true;
                            }
                        }
                    }
                    if (toVisitOffset == 0) break;
                    currentNodeIndex = nodesToVisit[--toVisitOffset];
                } else {
                    // Internal node
                    nodesToVisit[toVisitOffset++] = node->rightChildOffset;
                    currentNodeIndex = currentNodeIndex + 1;
                }
            } else {
                if (toVisitOffset == 0) break;
                currentNodeIndex = nodesToVisit[--toVisitOffset];
            }
        }
        return hitAny;
    }

    void Mesh::fillInteraction(const Ray& ray, SurfaceInteraction& isect) const {
        uint32_t i0 = m_indices[isect.primIndex * 3 + 0];
        uint32_t i1 = m_indices[isect.primIndex * 3 + 1];
        uint32_t i2 = m_indices[isect.primIndex * 3 + 2];

        const Point3f& p0 = m_positions[i0];
        const Point3f& p1 = m_positions[i1];
        const Point3f& p2 = m_positions[i2];

        // Baricentric coordinates
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
        }
    }

    Bounds3f Mesh::getBounds() const {
        return m_bounds;
    }

    std::string Mesh::toString() const {
        return std::format("Mesh[file={}, tris={}]", m_filename, m_indices.size()/3);
    }

    GND_REGISTER_CLASS(Mesh, "mesh");
}
