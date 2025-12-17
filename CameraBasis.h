//
// Created by Ilya Plisko on 17.12.25.
//

#ifndef RENDERING_PROJECT_CAMERABASIS_H
#define RENDERING_PROJECT_CAMERABASIS_H
#include "Vector3.h"


struct CameraBasis {
    Vector3 o; // origin
    Vector3 f; // forward (normalized)
    Vector3 r; // right (normalized)
    Vector3 u; // up (normalized)

    CameraBasis(const Vector3& origin, const Vector3& forward, const Vector3& up_hint) :
    o(origin), f(forward.normalized()), r(f.cross(up_hint).normalized()), u(r.cross(f).normalized()) {}

    [[nodiscard]] Vector3 pixelDir(unsigned x, unsigned y, unsigned width, unsigned height, double fov) const;
};


#endif //RENDERING_PROJECT_CAMERABASIS_H