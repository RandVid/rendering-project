//
// Created by Saba Tsirekidze on 15.12.25
//

#ifndef RENDERING_PROJECT_PLANE_H
#define RENDERING_PROJECT_PLANE_H

#include "Object.h"
#include <functional>
#include <utility>

struct Plane : public Object {
    Vector3 point;          // Any point on the plane
    Vector3 normal;         // Plane normal (should be normalized)
    std::function<sf::Color(const Vector3&)> color_func = [](const Vector3&) { return sf::Color::White; };

public:
    // Position + normal constructor
    Plane(const Vector3& point, const Vector3& normal)
        : point(point), normal(normal.normalized()) {}

    // Position + normal + color function
    Plane(const Vector3& point, const Vector3& normal, std::function<sf::Color(const Vector3&)> color_func)
        : point(point), normal(normal.normalized()), color_func(std::move(color_func)) {}

    // Position + normal + fixed color
    Plane(const Vector3& point, const Vector3& normal, sf::Color color)
        : Plane(point, normal, [color](const Vector3&) { return color; }) {}

    double distanceToSurface(const Vector3& p) override {
        // Signed distance from point to plane
        return (p - point).dot(normal);
    }

    Vector3 getNormalAt(const Vector3& /*p*/) override {
        return normal; // same normal everywhere
    }

    sf::Color getColorAt(const Vector3& p) override {
        return color_func(p);
    }

    // Getters for GPU upload
    const Vector3& getPoint() const { return point; }
    const Vector3& getNormal() const { return normal; }

    // Optional: setter to rotate the plane
    void setNormal(const Vector3& new_normal) {
        normal = new_normal.normalized();
    }

    void setPoint(const Vector3& new_point) {
        point = new_point;
    }


    Vector3 getCenterOrPoint() const override { return point; }
    float getRadiusOrSize() const override { return 0.0f; }
    sf::Color getColorAtOrigin() const override { return color_func(const_cast<Vector3&>(point)); }
    Vector3 getNormalAtOrigin() const override { return normal; }
};

#endif // RENDERING_PROJECT_PLANE_H
