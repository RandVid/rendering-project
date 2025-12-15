//
// Created by Ilya Plisko on 05.11.25.
//

#include "RayMarchingRender.h"
#include "Quaternion.h"
#include <thread>
#include <vector>


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


std::tuple<double, Vector3, Object &> RayMarchingRender::intersection(Ray ray) {
    double distance_marched = 0.0;
    unsigned steps = 0;
    double closest_distance = INFINITY;
    Object *closest_object = objects[0];
    while (closest_distance > 0.1 && distance_marched < 100.0 && steps < 50) {
        auto [dist, obj] = distanceToClosest(ray);
        closest_distance = dist;
        closest_object = obj;
        distance_marched += closest_distance;
        ray.move(ray.getDirection() * closest_distance);
        steps++;
    }
    if (closest_distance > 0.1)
        distance_marched = -1;
    return {distance_marched, ray.getOrigin(), *closest_object};
}


void RayMarchingRender::renderFrame(Ray ray) {
    auto xy_rotation = ray.getDirection().cross(Z);
    ray.rotate(fromAngleAxis(fov/2.0 * height / width, xy_rotation)*fromAngleAxis(-fov/2.0, Z));

    std::vector<sf::Vertex> vertex_vector(height * width);

    // Determine number of threads
    unsigned numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    unsigned rowsPerThread = (height + numThreads - 1) / numThreads;
    
    for (unsigned t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, rowsPerThread, ray, xy_rotation, &vertex_vector]() {
            unsigned startY = t * rowsPerThread;
            unsigned endY = std::min(startY + rowsPerThread, static_cast<unsigned>(height));
            
            // Each thread gets its own copy of the ray
            Ray threadRay = ray;
            
            // Skip to the starting row - need to apply end-of-row rotation for each skipped row
            // AND apply all the X rotations for each skipped row
            for (unsigned skip = 0; skip < startY; ++skip) {
                // Simulate going through all X pixels in this row
                for (unsigned x = 0; x < width; x++) {
                    threadRay.rotate(fromAngleAxis(fov/width, Z));
                }
                // Then apply end-of-row rotations
                threadRay.rotate(fromAngleAxis(-fov/2, Z));
                threadRay.rotate(fromAngleAxis(fov/width, xy_rotation*-1));
                threadRay.rotate(fromAngleAxis(-fov/2, Z));
            }
            
            // Process assigned rows
            for (unsigned y = startY; y < endY; y++) {
                for (unsigned x = 0; x < width; x++) {
                    auto [dist, point, object] = intersection(threadRay);
                    
                    sf::Vertex vertex(sf::Vector2f(static_cast<float>(x),
                                  static_cast<float>(y)));
                    
                    if (dist >= 0) {
                        Vector3 tr_normal = object.getNormalAt(point).normalized();
                        double brightness = (tr_normal.dot(threadRay.getDirection()) > 0) ?
                            tr_normal.dot(light.normalized()) :
                            (tr_normal*-1).dot(light.normalized());
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
                    threadRay.rotate(fromAngleAxis(fov/width, Z));
                }
                
                threadRay.rotate(fromAngleAxis(-fov/2, Z));
                threadRay.rotate(fromAngleAxis(fov/width, xy_rotation*-1));
                threadRay.rotate(fromAngleAxis(-fov/2, Z));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    window.draw(vertex_vector.data(), vertex_vector.size(), sf::PrimitiveType::Points);
}