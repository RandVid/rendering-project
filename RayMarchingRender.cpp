//
// Created by Ilya Plisko on 05.11.25.
//

#include "RayMarchingRender.h"
#include "Quaternion.h"
#include <thread>
#include <vector>

#include "CameraBasis.h"


std::pair<double, Object*> RayMarchingRender::distanceToClosest(Vector3& p) {
    double closest_distance = std::numeric_limits<double>::infinity();
    Object* closest_object = nullptr;

    for (auto* object : objects) {
        double dist = object->distanceToSurface(p);
        if (dist < closest_distance) {
            closest_distance = dist;
            closest_object = object;
        }
    }

    return {closest_distance, closest_object};
}



std::tuple<double, Vector3, Object&>
RayMarchingRender::intersection(const Vector3& origin, const Vector3& dir) {
    Vector3 pos = origin;
    double distance_marched = 0.0;

    constexpr double hit_epsilon  = 0.01;
    constexpr double max_distance = 200.0;
    constexpr unsigned max_steps  = 64;

    Object* hit_obj = nullptr;

    for (unsigned step = 0;
         step < max_steps && distance_marched < max_distance;
         ++step)
    {
        auto [d, obj] = distanceToClosest(pos);
        if (!obj) break;                // no objects in scene – safety

        if (d < hit_epsilon) {          // HIT: stop *before* marching past
            hit_obj = obj;
            break;
        }

        // Sphere-trace step
        pos += dir * d;                 // no temp
        distance_marched += d;
    }

    if (!hit_obj) {
        return {-1.0, pos, *objects[0]};
    }

    return {distance_marched, pos, *hit_obj};
}


void RayMarchingRender::renderFrame(Ray ray) {
    auto xy_rotation = ray.getDirection().cross(Z);
    // ray.rotate(fromAngleAxis(fov/2.0 * height / width, xy_rotation)*fromAngleAxis(-fov/2.0, Z));

    std::vector<sf::Vertex> vertex_vector(height * width);

    // Determine number of threads
    unsigned numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    CameraBasis cam = CameraBasis(ray.getOrigin(), ray.getDirection(), Z*-1);

    unsigned rowsPerThread = (height + numThreads - 1) / numThreads;
    light.normalize();

    for (unsigned t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, rowsPerThread, ray, xy_rotation, &vertex_vector, &cam]() {
            unsigned startY = t * rowsPerThread;
            unsigned endY = std::min(startY + rowsPerThread, static_cast<unsigned>(height));

            // Each thread gets its own copy of the ray
            Ray threadRay = ray;

            // Skip to the starting row - need to apply end-of-row rotation for each skipped row
            // AND apply all the X rotations for each skipped row
            // for (unsigned skip = 0; skip < startY; ++skip) {
            //     // Simulate going through all X pixels in this row
            //     for (unsigned x = 0; x < width; x++) {
            //         threadRay.rotate(fromAngleAxis(fov/width, Z));
            //     }
            //     // Then apply end-of-row rotations
            //     threadRay.rotate(fromAngleAxis(-fov/2, Z));
            //     threadRay.rotate(fromAngleAxis(fov/width, xy_rotation*-1));
            //     threadRay.rotate(fromAngleAxis(-fov/2, Z));
            // }

            // Process assigned rows
            for (unsigned y = startY; y < endY; y++) {
                for (unsigned x = 0; x < width; x++) {
                    Vector3 dir = cam.pixelDir(x, y, width, height, fov);
                    auto [dist, point, object] = intersection(threadRay.getOrigin(),
                        dir);

                    sf::Vertex vertex(sf::Vector2f(static_cast<float>(x),
                                  static_cast<float>(y)));

                    if (dist >= 0) {
                        Vector3 tr_normal = object.getNormalAt(point).normalized();
                        double brightness = tr_normal.dot(light);
                        brightness = std::min(std::max(brightness, 0.0)+0.3, 1.0);
                        const auto colorAt = object.getColorAt(point);

                        vertex.color = {
                            static_cast<std::uint8_t>(colorAt.r*brightness),
                            static_cast<std::uint8_t>(colorAt.g*brightness),
                            static_cast<std::uint8_t>(colorAt.b*brightness)
                        };
                    }
                    else {
                        vertex.color = {50, 50, 50};
                    }

                    vertex_vector[y * width + x] = vertex;
                    // threadRay.rotate(fromAngleAxis(fov/width, Z));
                }

                // threadRay.rotate(fromAngleAxis(-fov/2, Z));
                // threadRay.rotate(fromAngleAxis(fov/width, xy_rotation*-1));
                // threadRay.rotate(fromAngleAxis(-fov/2, Z));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    window.draw(vertex_vector.data(), vertex_vector.size(), sf::PrimitiveType::Points);
}

