#pragma once

#include <gianduia/core/object.h>
#include <cmath>
#include <numeric>

namespace gnd {

    class Perlin {
    public:
        /// Computes 3D Perlin Noise at a given point. Returns a value in [-1, 1].
        static float noise(const Point3f& p) {
            int X = static_cast<int>(std::floor(p.x())) & 255;
            int Y = static_cast<int>(std::floor(p.y())) & 255;
            int Z = static_cast<int>(std::floor(p.z())) & 255;

            float x = p.x() - std::floor(p.x());
            float y = p.y() - std::floor(p.y());
            float z = p.z() - std::floor(p.z());

            float u = fade(x);
            float v = fade(y);
            float w = fade(z);

            int A  = P[X] + Y;
            int AA = P[A] + Z;
            int AB = P[A + 1] + Z;
            int B  = P[X + 1] + Y;
            int BA = P[B] + Z;
            int BB = P[B + 1] + Z;

            float grad000 = grad(P[AA],     x,        y,        z);
            float grad100 = grad(P[BA],     x - 1.0f, y,        z);
            float grad010 = grad(P[AB],     x,        y - 1.0f, z);
            float grad110 = grad(P[BB],     x - 1.0f, y - 1.0f, z);
            float grad001 = grad(P[AA + 1], x,        y,        z - 1.0f);
            float grad101 = grad(P[BA + 1], x - 1.0f, y,        z - 1.0f);
            float grad011 = grad(P[AB + 1], x,        y - 1.0f, z - 1.0f);
            float grad111 = grad(P[BB + 1], x - 1.0f, y - 1.0f, z - 1.0f);

            float x00 = Lerp(u, grad000, grad100);
            float x10 = Lerp(u, grad010, grad110);
            float x01 = Lerp(u, grad001, grad101);
            float x11 = Lerp(u, grad011, grad111);

            float y0 = Lerp(v, x00, x10);
            float y1 = Lerp(v, x01, x11);

            return Lerp(w, y0, y1);
        }

        /// Fractional Brownian Motion (fBm)
        /// Sums multiple octaves of noise.
        /// Returns a value in [0, 1]
        static float fBm(const Point3f& p, int octaves) {
            float accum = 0.0f;
            Point3f temp = p;
            float weight = 0.5f;

            for (int i = 0; i < octaves; ++i) {
                accum += weight * noise(temp);
                weight *= 0.5f;
                temp *= 2.0f;
            }

            return std::clamp((accum + 1.0f) * 0.5f, 0.0f, 1.0f);
        }

        /// Uses the absolute value of noise to create peaks (veins).
        /// Returns a value in [0, 1].
        static float turbulence(const Point3f& p, int octaves) {
            float accum = 0.0f;
            Point3f temp = p;
            float weight = 1.0f;

            for (int i = 0; i < octaves; ++i) {
                accum += weight * std::abs(noise(temp));
                weight *= 0.5f;
                temp *= 2.0f;
            }

            return std::clamp(accum, 0.0f, 1.0f);
        }

    private:
        static float fade(float t) {
            // 6t^5 - 15t^4 + 10t^3
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        static float grad(int hash, float x, float y, float z) {
            int h = hash & 15;
            float u = h < 8 ? x : y;
            float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
            return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
        }

        // Permutation table (0-255, repeated twice to avoid modulos).
        inline static const int P[512] = {
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
            190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,
            20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,
            230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,
            169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,
            147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,
            2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,
            112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
            107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,
            114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
            // Repetition
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
            190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,
            20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,
            230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,
            169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,
            147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,
            2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,
            112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
            107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,
            114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
        };
    };

}