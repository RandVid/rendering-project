//
// Created by user on 3/12/2025.
//

#ifndef VECTOR3_H
#define VECTOR3_H
// #include "Quaternion.h"
// #include "Quaternion.h"


class Quaternion;

class Vector3 {
private:
    double x, y, z;

public:
    Vector3(double x, double y, double z) : x(x), y(y), z(z) {}
    Vector3(const Vector3& v) : x(v.x), y(v.y), z(v.z) {}
    Vector3() : x(0), y(0), z(0) {}

    [[nodiscard]] double getX() const { return x; }
    [[nodiscard]] double getY() const { return y; }
    [[nodiscard]] double getZ() const { return z; }

    Vector3 operator+ (const Vector3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vector3 operator- (const Vector3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    [[nodiscard]] double dot (const Vector3& other) const { return x*other.x+y*other.y+z*other.z; }
    [[nodiscard]] Vector3 cross (const Vector3& other) const { return {y*other.z - z*other.y, z*other.x - x*other.z, x*other.y - y*other.x}; }

    Vector3& operator+= (const Vector3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vector3& operator-= (const Vector3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vector3& applyCross (const Vector3& other) {
        x = y*other.z - z*other.y;
        y = z*other.x - x*other.z;
        z = x*other.y - y*other.x;
        return *this;
    }

    Vector3 operator+ (double scalar) const { return {x + scalar, y + scalar, z + scalar}; }
    Vector3 operator- (double scalar) const { return {x - scalar, y - scalar, z - scalar}; }
    Vector3 operator* (const double scalar) const {return {x*scalar, y*scalar, z*scalar}; }
    Vector3 operator/ (const double scalar) const { return {x/scalar, y/scalar, z/scalar}; }

    Vector3& operator+= (const double scalar) { x += scalar; y += scalar; z += scalar; return *this; }
    Vector3& operator-= (const double scalar) { x -= scalar; y -= scalar; z -= scalar; return *this; }
    Vector3& operator*= (const double scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    Vector3& operator/= (const double scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

    [[nodiscard]] double magnitude() const ;
    [[nodiscard]] Vector3 normalized() const;
    Vector3& normalize();
    [[nodiscard]] Vector3 rotated(const Quaternion& angle) const;
    Vector3& rotate(const Quaternion& angle);
};



#endif //VECTOR3_H
