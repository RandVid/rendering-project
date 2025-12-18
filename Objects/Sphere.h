//
// Created by Ilya Plisko on 14.12.25.
//

#ifndef RENDERING_PROJECT_SPHERE_H
#define RENDERING_PROJECT_SPHERE_H
#include <functional>
#include <utility>

#include "Object.h"


class Sphere : public Object {
    Vector3 center;
    double radius;
    std::function<sf::Color(const Vector3&)> color_func = [](const Vector3&){ return sf::Color::White; };
    public:
    Sphere(const Vector3& center, double radius) : center(center), radius(radius) {}
    Sphere(const Vector3& center, double radius, std::function<sf::Color(const Vector3&)> color_func) :
        center(center), radius(radius), color_func(std::move(color_func)) {}
    Sphere(const Vector3& center, double radius, sf::Color color) :
        Sphere(center, radius, [color](const Vector3&){ return color; }) {}
    double distanceToSurface(const Vector3& point) override { return (point - center).magnitude() - radius; }
    Vector3 getNormalAt(const Vector3& point) override { return (point - center).normalized(); }
    sf::Color getColorAt(const Vector3& point) override { return color_func(point); }
    // Getters for GPU upload
    const Vector3& getCenter() const { return center; }
    double getRadius() const { return radius; }

    Vector3 getCenterOrPoint() const override { return center; }
    float getRadiusOrSize() const override { return radius; }
    sf::Color getColorAtOrigin() const override { return color_func(const_cast<Vector3&>(center)); }
    Vector3 getNormalAtOrigin() const override { return (Vector3(0,0,0)); }
};


#endif //RENDERING_PROJECT_SPHERE_H