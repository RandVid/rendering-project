//
// Created by Ilya Plisko on 29.11.25.
//

#ifndef RENDERING_PROJECT_OBJECT_H
#define RENDERING_PROJECT_OBJECT_H
#include "../Vector3.h"
#include "SFML/Graphics/Color.hpp"


class Vector3;

struct Object {
    virtual ~Object() = default;

    virtual double distanceToSurface(Vector3&) = 0;
    virtual sf::Color getColorAt(Vector3&) = 0;
    virtual Vector3 getNormalAt(Vector3&) = 0;
};


#endif //RENDERING_PROJECT_OBJECT_H