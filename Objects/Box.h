#ifndef BOX_H
#define BOX_H

#include "Object.h"
#include "SDFUtils.h"

class Box : public Object {
    Vector3 center;
    Vector3 halfSize;
    sf::Color color;

public:
    Box(const Vector3& c, const Vector3& hs, sf::Color col)
        : center(c), halfSize(hs), color(col) {}

    double distanceToSurface(const Vector3& p) override {
        Vector3 q = absVec(p - center) - halfSize;
        Vector3 mq = maxVec(q, 0.0);
        double outside = mq.magnitude();
        double inside = std::min(
            std::max({q.getX(), q.getY(), q.getZ()}),
            0.0
        );
        return outside + inside;
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
    float getRadiusOrSize() const override { return halfSize.getX(); } // assume uniform size
    Vector3 getSize() const { return halfSize; }
    sf::Color getColorAtOrigin() const override { return color; }
    Vector3 getNormalAtOrigin() const override { return Vector3(0,0,0); } // shader will compute
};

#endif
