//
// Created by user on 3/12/2025.
//

#ifndef QUATERNION_H
#define QUATERNION_H

#include "Vector3.h"
#include <cmath>

class Vector3;

class Quaternion {
    double w;
    Vector3 v;

    public:
    Quaternion(double w, double x, double y, double z);
    Quaternion(double w, const Vector3& v);
    explicit Quaternion(const Vector3& v);
    Quaternion (const Quaternion& q);

    [[nodiscard]] Vector3 getVector() const;
    [[nodiscard]] double getW() const;

    Quaternion operator+(const Quaternion& q) const;
    Quaternion operator-(const Quaternion& q) const;
    Quaternion operator*(const Quaternion& q) const;
    Quaternion operator*(const Vector3& vec) const;

    Quaternion operator*(double scalar) const;
    Quaternion operator/(double scalar) const;

    [[nodiscard]] double magnitude() const;
    [[nodiscard]] Quaternion normalize() const;
    [[nodiscard]] Quaternion conjugate() const;
    [[nodiscard]] Quaternion inverse() const;
};

Quaternion fromAngleAxis(double angle, const Vector3& dir);


#endif //QUATERNION_H
