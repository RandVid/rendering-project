#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "../Objects/SDFUtils.h"
#include "../Objects/Object.h"

class Difference : public Object {
    Object* a;
    Object* b;

public:
    Difference(Object* a, Object* b) : a(a), b(b) {}

    Object* getA() const { return a; }
    Object* getB() const { return b; }

    double distanceToSurface(const Vector3& p) override {
        return std::max(a->distanceToSurface(p), -b->distanceToSurface(p));
    }

    Vector3 getNormalAt(const Vector3& p) override {
        return a->getNormalAt(p);
    }

    sf::Color getColorAt(const Vector3& p) override {
        return a->getColorAt(p);
    }

    Vector3 getCenterOrPoint() const override { return Vector3(0,0,0); }
    float getRadiusOrSize() const override { return 0.0f; }
    sf::Color getColorAtOrigin() const override { return sf::Color::White; }
    Vector3 getNormalAtOrigin() const override { return Vector3(0,0,0); }
};

#endif
