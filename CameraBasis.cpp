//
// Created by Ilya Plisko on 17.12.25.
//

#include "CameraBasis.h"
#include <cmath>

Vector3 CameraBasis::pixelDir(const unsigned x, const unsigned y, const unsigned width, const unsigned height, const double fov) const {
    const double aspect = static_cast<double>(width) / static_cast<double>(height);
    const double ndc_x = ( (x + 0.5) / static_cast<double>(width)  ) * 2.0 - 1.0;
    const double ndc_y = ( (y + 0.5) / static_cast<double>(height) ) * 2.0 - 1.0;

    const double tanHalfFov = std::tan(fov/2);
    Vector3 dir = (f
                   + r * (ndc_x * tanHalfFov)
                   - u * (ndc_y * tanHalfFov / aspect))
                  .normalized();
    return dir;
}

