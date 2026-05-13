#pragma once

#include <gianduia/core/object.h>
#include <cmath>
#include <algorithm>

namespace gnd {

    struct WorleyResult {
        float f1;
        float f2;
    };

    class Worley {
    public:
        static WorleyResult noise(const Point3f& p) {
            int x0 = static_cast<int>(std::floor(p.x()));
            int y0 = static_cast<int>(std::floor(p.y()));
            int z0 = static_cast<int>(std::floor(p.z()));

            float minDist1 = 1e8f;
            float minDist2 = 1e8f;

            for (int k = -1; k <= 1; ++k) {
                for (int j = -1; j <= 1; ++j) {
                    for (int i = -1; i <= 1; ++i) {
                        int cx = x0 + i;
                        int cy = y0 + j;
                        int cz = z0 + k;

                        Point3f featurePoint = Point3f(cx, cy, cz) + hash3(cx, cy, cz);
                        float dist = (p - featurePoint).length();

                        if (dist < minDist1) {
                            minDist2 = minDist1;
                            minDist1 = dist;
                        } else if (dist < minDist2) {
                            minDist2 = dist;
                        }
                    }
                }
            }

            return { minDist1, minDist2 };
        }

    private:
        static Point3f hash3(int x, int y, int z) {
            auto hash1 = [](uint32_t n) {
                n = (n << 13U) ^ n;
                n = n * (n * n * 15731U + 789221U) + 1376312589U;
                return float(n & 0x7fffffffU) / float(0x7fffffff);
            };

            uint32_t n = x * 73856093U ^ y * 19349663U ^ z * 83492791U;
            return Point3f(hash1(n), hash1(n + 1), hash1(n + 2));
        }
    };

}