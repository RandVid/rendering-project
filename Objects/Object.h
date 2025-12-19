//
// Created by Ilya Plisko on 29.11.25.
//

#ifndef RENDERING_PROJECT_OBJECT_H
#define RENDERING_PROJECT_OBJECT_H
#include "../Vector3.h"
#include "SFML/Graphics/Color.hpp"
#include "SDFUtils.h"


class Vector3;

struct Object {
    virtual ~Object() = default;

    virtual double distanceToSurface(const Vector3&) = 0;
    virtual sf::Color getColorAt(const Vector3&) = 0;
    virtual Vector3 getNormalAt(const Vector3&) = 0;

    virtual Vector3 getCenterOrPoint() const { return Vector3(0,0,0); }
    virtual float getRadiusOrSize() const { return 0.0f; }
    virtual sf::Color getColorAtOrigin() const { return sf::Color::White; }
    virtual Vector3 getNormalAtOrigin() const { return Vector3(0,1,0); }
    virtual float getReflectivity() const { return 0.0f; }  // Default: no reflection
};


#endif //RENDERING_PROJECT_OBJECT_H