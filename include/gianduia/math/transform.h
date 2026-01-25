#pragma once

#include <gianduia/math/geometry.h>
#include <gianduia/math/ray.h>
#include <gianduia/math/bounds.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gnd {

    class Transform {
    private:
        glm::mat4 m;
        glm::mat4 mInv;

    public:

        Transform() : m(1.0f), mInv(1.0f) {}
        explicit Transform(const glm::mat4& mat) : m(mat), mInv(glm::inverse(mat)) {}
        Transform(const glm::mat4& mat, const glm::mat4& inv) : m(mat), mInv(inv) {}

        const glm::mat4& GetMatrix() const { return m; }
        const glm::mat4& GetInverse() const { return mInv; }

        bool operator==(const Transform& t) const { return m == t.m; }
        bool operator!=(const Transform& t) const { return m != t.m; }

        bool IsIdentity() const {
            return m == glm::mat4(1.0f);
        }

        Transform Inverse() const {
            return Transform(mInv, m);
        }

        Transform operator*(const Transform& t2) const {
            return Transform(m * t2.m, t2.mInv * mInv);
        }

        /* Applying transform to points, vectors, normals, rays and AABBs */

        Point3f operator()(const Point3f& p) const {
            glm::vec4 input(p.p, 1.0f);
            glm::vec4 output = m * input;

            if (output.w == 1.0f)
                return Point3f(glm::vec3(output));          // Affine transformation
            return Point3f(glm::vec3(output) / output.w);   // Perspective division
        }

        Vector3f operator()(const Vector3f& v) const {
            glm::vec4 input(v.v, 0.0f);
            glm::vec4 output = m * input;
            return Vector3f(glm::vec3(output));
        }

        Normal3f operator()(const Normal3f& n) const {
            glm::mat3 normMat = glm::transpose(glm::mat3(mInv));
            return Normal3f(normMat * n.n);
        }

        Ray operator()(const Ray& r) const {
            Point3f o = (*this)(r.o);
            Vector3f d = (*this)(r.d);
            return Ray(o, d, r.tMax);
        }

        Bounds3f operator()(const Bounds3f& b) const {
            Bounds3f ret( (*this)(b.corner(0)) );

            for (int i = 1; i < 8; ++i) {
                ret = Union(ret, (*this)(b.corner(i)));
            }
            return ret;
        }

        /* Static factories */

        static Transform Translate(const Vector3f& delta) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), delta.v);
            glm::mat4 mInv = glm::translate(glm::mat4(1.0f), -delta.v);
            return Transform(m, mInv);
        }

        static Transform Scale(float x, float y, float z) {
            glm::mat4 m = glm::scale(glm::mat4(1.0f), glm::vec3(x, y, z));
            glm::mat4 mInv = glm::scale(glm::mat4(1.0f), glm::vec3(1.f/x, 1.f/y, 1.f/z));
            return Transform(m, mInv);
        }

        static Transform RotateX(float degrees) {
            float rad = glm::radians(degrees);
            glm::mat4 m = glm::rotate(glm::mat4(1.0f), rad, glm::vec3(1, 0, 0));
            glm::mat4 mInv = glm::transpose(m);
            return Transform(m, mInv);
        }

        static Transform RotateY(float degrees) {
            float rad = glm::radians(degrees);
            glm::mat4 m = glm::rotate(glm::mat4(1.0f), rad, glm::vec3(0, 1, 0));
            glm::mat4 mInv = glm::transpose(m);
            return Transform(m, mInv);
        }

        static Transform RotateZ(float degrees) {
            float rad = glm::radians(degrees);
            glm::mat4 m = glm::rotate(glm::mat4(1.0f), rad, glm::vec3(0, 0, 1));
            glm::mat4 mInv = glm::transpose(m);
            return Transform(m, mInv);
        }

        static Transform Rotate(float degrees, const Vector3f& axis) {
            float rad = glm::radians(degrees);
            glm::mat4 m = glm::rotate(glm::mat4(1.0f), rad, axis.v);
            glm::mat4 mInv = glm::transpose(m);
            return Transform(m, mInv);
        }

        static Transform LookAt(const Point3f& pos, const Point3f& target, const Vector3f& up) {
            glm::mat4 view = glm::lookAt(pos.p, target.p, up.v);
            return Transform(glm::inverse(view), view); // m=CameraToWorld, mInv=WorldToCamera
        }

        static Transform Perspective(float fov, float aspect, float zNear, float zFar) {
            glm::mat4 m = glm::perspective(glm::radians(fov), aspect, zNear, zFar);
            return Transform(m, glm::inverse(m));
        }
    };

}