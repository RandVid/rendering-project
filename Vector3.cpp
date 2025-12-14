//
// Created by user on 3/12/2025.
//

#include "Vector3.h"
#include "Quaternion.h"

#include <cmath>
#include <stdexcept>


double Vector3::magnitude() const {
    return sqrt(x*x + y*y + z*z);
}


Vector3 Vector3::normalized() const {
    double magnitude = Vector3::magnitude();
    if (magnitude == 0) {
        throw std::invalid_argument("magnitude cannot be zero");
    }
    return *this/magnitude;
}

Vector3& Vector3::normalize() {
    double magnitude = Vector3::magnitude();
    if (magnitude == 0) {
        throw std::invalid_argument("magnitude cannot be zero");
    }
    *this /= magnitude;
    return *this;
}

Vector3& Vector3::rotate(const Quaternion& angle) {
    // Perform the rotation: q * v * q^-1
    *this = (angle * *this * angle.inverse()).getVector();

    // Extract the rotated vector from the resulting quaternion
    return *this;
}

Vector3 Vector3::rotated(const Quaternion& angle) const {
    // Perform the rotation: q * v * q^-1
    // Extract the rotated vector from the resulting quaternion
    return (angle * *this * angle.inverse()).getVector();
}