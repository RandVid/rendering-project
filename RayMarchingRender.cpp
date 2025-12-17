//
// Created by Ilya Plisko on 05.11.25.
//

#include "RayMarchingRender.h"
#include "Quaternion.h"
#include <thread>
#include <vector>

#include "CameraBasis.h"


std::pair<double, Object*> RayMarchingRender::distanceToClosest(Ray &ray) {
    double closest_distance = INFINITY;
    Object *closest_object = objects[0];
    for (auto &object : objects) {
        if (const auto dist = object->distanceToSurface(ray.getOrigin()); dist < closest_distance) {
            closest_distance = dist;
            closest_object = object;
        }
    }
    return {closest_distance, closest_object};
}


std::tuple<double, Vector3, Object &> RayMarchingRender::intersection(Vector3& origin, Vector3& dir) {
    Vector3 pos = {origin};
    double distance_marched = 0.0;
    unsigned steps = 0;

    double closest_distance = std::numeric_limits<double>::infinity();
    Object* closest_object = objects[0];

    // Tunables
    constexpr double hit_epsilon = 0.001; // tighter eps speeds up and improves accuracy
    constexpr double max_distance = 200.0; // increase if your scene is large
    constexpr unsigned max_steps = 128;    // allow more steps if needed

    while (closest_distance > hit_epsilon && distance_marched < max_distance && steps < max_steps) {
        // SDF query at current point
        double d = std::numeric_limits<double>::infinity();
        Object* obj = objects[0];
        for (auto* o : objects) {
            double od = o->distanceToSurface(pos);
            if (od < d) { d = od; obj = o; }
        }

        closest_distance = d;
        closest_object = obj;

        // Sphere-trace step
        pos += dir * closest_distance;
        distance_marched += closest_distance;
        ++steps;
    }

    if (closest_distance > hit_epsilon) {
        return {-1.0, pos, *closest_object};
    }
    return {distance_marched, pos, *closest_object};
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

