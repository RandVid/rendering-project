//
// Created by Ilya Plisko on 05.11.25.
//

#include "RayMarchingRender.h"
#include "Quaternion.h"



std::pair<double, Object &> RayMarchingRender::distanceToClosest(Ray &ray) {
    double closest_distance = INFINITY;
    Object &closest_object = *objects[0];
    for (auto &object : objects) {
        if (const auto dist = object->distanceToSurface(ray.getOrigin()); dist < closest_distance) {
            closest_distance = dist;
            closest_object = *object;
        }
    }
    return {closest_distance, closest_object};
}


std::tuple<double, Vector3, Object &> RayMarchingRender::intersection(Ray ray) {
    double distance_marched = 0.0;
    unsigned steps = 0;
    double closest_distance = INFINITY;
    Object &closest_object = *objects[0];
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
    return {distance_marched, ray.getOrigin(), closest_object};
}


void RayMarchingRender::renderFrame(Ray ray) {
    auto xy_rotation = ray.getDirection().cross(Z);
    ray.rotate(fromAngleAxis(fov/2.0 * height / width, xy_rotation)*fromAngleAxis(-fov/2.0, Z));

    std::vector<sf::Vertex> vertex_vector = {};
    vertex_vector.reserve(height*width);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto [dist, point, object] = intersection(ray);
            if (dist >= 0) {
                Vector3 tr_normal = object.getNormalAt(point).normalized();
                double brightness = (tr_normal.dot(ray.getDirection()) > 0) ?
                    tr_normal.dot(light.normalized()) :
                    (tr_normal*-1).dot(light.normalized());
                brightness = std::min(std::max(brightness, 0.0)+0.3, 1.0);
                const auto colorAt = object.getColorAt(point);
                sf::Vertex vertex(sf::Vector2f(static_cast<float>(x),
                              static_cast<float>(y)),{
                        static_cast<std::uint8_t>(colorAt.r*brightness),
                        static_cast<std::uint8_t>(colorAt.g*brightness),
                        static_cast<std::uint8_t>(colorAt.b*brightness)}
                    );
                vertex_vector.push_back(vertex);
            }
            else {
                sf::Vertex vertex(sf::Vector2f(static_cast<float>(x),
                              static_cast<float>(y)),{
                        50,
                        50,
                        50}
                    );
                vertex_vector.push_back(vertex);
            }
            ray.rotate(fromAngleAxis(fov/width, Z));
        }

        ray.rotate(fromAngleAxis(-fov/2, Z));
        ray.rotate(fromAngleAxis(fov/width, xy_rotation*-1));
        ray.rotate(fromAngleAxis(-fov/2, Z));
    }
    window.draw(vertex_vector.data(), vertex_vector.size(), sf::PrimitiveType::Points);
}

