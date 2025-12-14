//
// Created by user on 3/12/2025.
//

#ifndef RAY_H
#define RAY_H
#include <iostream>

#include "Vector3.h"
#include "Quaternion.h"

class Ray {
    Vector3 origin;
    Vector3 direction;

    public:
    Ray(const Vector3& origin, const Vector3& direction) : origin(origin), direction(direction.normalized()) {}
    Ray(const Ray& ray) : origin(ray.origin), direction(ray.direction) {}

    void setOrigin(const Vector3& vector) { origin = {vector}; }
    void setDirection(const Vector3& vector) { direction = {vector}; }
    [[nodiscard]] Vector3& getDirection() { return direction; }
    [[nodiscard]] Vector3& getOrigin() { return origin; }

    void move(const Vector3& vector) { origin = origin + vector; }
    void rotate(const Quaternion& angle) {
        direction.rotate(angle);
    }
};



#endif //RAY_H
