//
// Created by user on 3/12/2025.
//

#include "Quaternion.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

Quaternion::Quaternion(const double w, const double x, const double y, const double z) : w(w), v(x, y, z) {};
Quaternion::Quaternion(const double w, const Vector3& v) : w(w), v(v) {};
Quaternion::Quaternion(const Vector3& v) : Quaternion(0, v) {}
Quaternion::Quaternion (const Quaternion& q) : Quaternion(q.w, q.v) {}

[[nodiscard]] Vector3 Quaternion::getVector() const {return {v}; }
[[nodiscard]] double Quaternion::getW() const {return w; };

Quaternion Quaternion::operator+(const Quaternion& q) const { return {w + q.w, v + q.v}; }
Quaternion Quaternion::operator-(const Quaternion& q) const { return {w - q.w, v - q.v}; }
Quaternion Quaternion::operator*(const Quaternion& q) const {
    return {
        w * q.w - v.dot(q.v), // Scalar part
        q.v * w + v * q.w + v.cross(q.v) // Vector part
    };
}
Quaternion Quaternion::operator*(const Vector3& vec) const {return {-v.dot(vec), vec*w + v.cross(vec)}; }

Quaternion Quaternion::operator*(const double scalar) const { return {w*scalar, v*scalar}; }
Quaternion Quaternion::operator/(const double scalar) const { return {w/scalar, v/scalar}; }

double Quaternion::magnitude() const {
    double vm = v.magnitude();
    return sqrt(w*w + vm*vm);
}


Quaternion Quaternion::normalize() const {
    double magnitude = Quaternion::magnitude();
    if (magnitude == 0) {
        throw std::invalid_argument("magnitude cannot be zero");
    }
    return {w/magnitude, v/magnitude};
}

Quaternion Quaternion::conjugate() const {
    return {w, v*-1};
}

Quaternion Quaternion::inverse() const {
    double magnitude = Quaternion::magnitude();
    Quaternion conjugated = conjugate();
    if (magnitude == 1) {
        return conjugated;
    }
    return conjugated / (magnitude*magnitude);
}

Quaternion fromAngleAxis(double angle, const Vector3& dir) {
    return {cos(angle/2), dir.normalized()*sin(angle/2)};
};

