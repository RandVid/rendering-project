#ifndef SDF_UTILS_H
#define SDF_UTILS_H

#include <algorithm>
#include <cmath>

inline double clamp(double x, double a, double b) {
    return std::max(a, std::min(b, x));
}

inline Vector3 absVec(const Vector3& v) {
    return {
        std::abs(v.getX()),
        std::abs(v.getY()),
        std::abs(v.getZ())
    };
}

inline Vector3 maxVec(const Vector3& v, double m) {
    return {
        std::max(v.getX(), m),
        std::max(v.getY(), m),
        std::max(v.getZ(), m)
    };
}

#endif // SDF_UTILS_H