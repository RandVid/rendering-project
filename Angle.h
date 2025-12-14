//
// Created by user on 3/13/2025.
//

#ifndef ANGLE_H
#define ANGLE_H
#include "Quaternion.h"
#include <cmath>


struct Angle : public Quaternion {
    Angle(const double angle, const Vector3& dir) : Quaternion(cos(angle/2), dir.normalized()*sin(angle/2)) {}
};



#endif //ANGLE_H
