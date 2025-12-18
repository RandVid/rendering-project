#ifndef CYLINDER_H
#define CYLINDER_H

#include "SDFUtils.h"
#include "Object.h"

class Cylinder : public Object {
    Vector3 center;
    double radius;
    double halfHeight;
    sf::Color color;

public:
    Cylinder(const Vector3& c, double r, double h, sf::Color col)
        : center(c), radius(r), halfHeight(h), color(col) {}

    double distanceToSurface(const Vector3& p) override {
        Vector3 q = p - center;
        double dxz = sqrt(q.getX()*q.getX() + q.getZ()*q.getZ()) - radius;
        double dy  = abs(q.getY()) - halfHeight;

        double outside = sqrt(
            std::max(dxz, 0.0)*std::max(dxz, 0.0) +
            std::max(dy,  0.0)*std::max(dy,  0.0)
        );

        return std::min(std::max(dxz, dy), 0.0) + outside;
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

    // GPU-friendly getters
    Vector3 getCenterOrPoint() const override { return center; }
    float getRadiusOrSize() const override { return radius; }
    float getHeight() const { return halfHeight; }
    sf::Color getColorAtOrigin() const override { return color; }
    Vector3 getNormalAtOrigin() const override { return Vector3(0,0,0); } // shader computes
};

#endif
