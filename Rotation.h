//
// Created by Ilya Plisko on 28.11.25.
//

#ifndef RENDERING_PROJECT_ROTATION_H
#define RENDERING_PROJECT_ROTATION_H
#include "Vector3.h"
#include <cmath>

#include "Quaternion.h"


struct Rotation : Quaternion {
    Rotation(const double angle, const Vector3& dir) : Quaternion(cos(angle/2), dir.normalized()*sin(angle/2)) {}
};

#endif //RENDERING_PROJECT_ROTATION_H