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
    std::function<sf::Color(Vector3&)> color_func = [](const Vector3&){ return sf::Color::White; };
    public:
    Sphere(const Vector3& center, double radius) : center(center), radius(radius) {}
    Sphere(const Vector3& center, double radius, std::function<sf::Color(Vector3&)> color_func) :
        center(center), radius(radius), color_func(std::move(color_func)) {}
    Sphere(const Vector3& center, double radius, sf::Color color) :
        Sphere(center, radius, [color](const Vector3&){ return color; }) {}
    double distanceToSurface(Vector3& point) override { return (point - center).magnitude() - radius; }
    Vector3 getNormalAt(Vector3& point) override { return (point - center).normalized(); }
    sf::Color getColorAt(Vector3& point) override { return color_func(point); }
    // Getters for GPU upload
    const Vector3& getCenter() const { return center; }
    double getRadius() const { return radius; }
};


#endif //RENDERING_PROJECT_SPHERE_H