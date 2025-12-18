#ifndef TORUS_H
#define TORUS_H

#include "SDFUtils.h"
#include "Object.h"

class Torus : public Object {
    Vector3 center;
    double majorR;
    double minorR;
    sf::Color color;

public:
    Torus(const Vector3& c, double R, double r, sf::Color col)
        : center(c), majorR(R), minorR(r), color(col) {}

    double distanceToSurface(const Vector3& p) override {
        Vector3 q = p - center;
        double xz = std::sqrt(q.getX()*q.getX() + q.getZ()*q.getZ()) - majorR;
        return std::sqrt(xz*xz + q.getY()*q.getY()) - minorR;
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

    double getMajorRadius() const {
        return majorR;
    }

    double getMinorRadius() const {
        return minorR;
    }
};

#endif
