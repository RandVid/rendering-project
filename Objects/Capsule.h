#ifndef CAPSULE_H
#define CAPSULE_H

#include "SDFUtils.h"
#include "Object.h"

class Capsule : public Object {
    Vector3 a, b;
    double radius;
    sf::Color color;

public:
    Capsule(const Vector3& a, const Vector3& b, double r, sf::Color c)
        : a(a), b(b), radius(r), color(c) {}

    double distanceToSurface(const Vector3& p) override {
        Vector3 pa = p - a;
        Vector3 ba = b - a;
        double h = clamp(pa.dot(ba) / ba.dot(ba), 0.0, 1.0);
        return (pa - ba * h).magnitude() - radius;
    }

    Vector3 getNormalAt(const Vector3& p) override {
        const double e = 1e-5;
        return Vector3(
            distanceToSurface(p + Vector3(e,0,0)) - distanceToSurface(p - Vector3(e,0,0)),
            distanceToSurface(p + Vector3(0,e,0)) - distanceToSurface(p - Vector3(0,e,0)),
            distanceToSurface(p + Vector3(0,0,e)) - distanceToSurface(p - Vector3(0,0,e))
        ).normalized();
    }

    sf::Color getColorAt(const Vector3&) override {
        return color;
    }

    double getHeight() const {
        return (b - a).magnitude();
    }
};

#endif
