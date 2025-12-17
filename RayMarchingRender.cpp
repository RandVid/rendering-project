//
// Created by Ilya Plisko on 05.11.25.
//

#include "RayMarchingRender.h"
#include "Quaternion.h"
#include <thread>
#include <vector>

#include "CameraBasis.h"
#include "Objects/Sphere.h"
#include "Objects/Plane.h"


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
    // GPU path: ensure shader loaded
    if (!ensureShaderLoaded()) {
        return; // fallback: shader failed to load
    }

    // Prepare camera basis
    Vector3 camOrigin = ray.getOrigin();
    Vector3 camForward = ray.getDirection().normalized();
    Vector3 camRight = camForward.cross(Z).normalized();
    if (camRight.magnitude() == 0) { // fallback if forward parallel to Z
        camRight = Vector3(1,0,0);
    }
    Vector3 camUp = camRight.cross(camForward).normalized();

    // Gather object data (limit to MAX_OBJECTS)
    unsigned count = std::min<unsigned>(objects.size(), MAX_OBJECTS);
    std::vector<sf::Glsl::Vec3> objPos(count);
    std::vector<float> objRadius(count);
    std::vector<sf::Glsl::Vec3> objColor(count);
    std::vector<float> objType(count);
    std::vector<sf::Glsl::Vec3> objNormal(count);

    for (unsigned i = 0; i < count; ++i) {
        Object* o = objects[i];
        if (auto* sph = dynamic_cast<class Sphere*>(o)) {
            objType[i] = 0.0f;
            objPos[i] = sf::Glsl::Vec3(static_cast<float>(sph->getCenter().getX()), static_cast<float>(sph->getCenter().getY()), static_cast<float>(sph->getCenter().getZ()));
            objRadius[i] = static_cast<float>(sph->getRadius());
            sf::Color c = sph->getColorAt(*const_cast<Vector3*>(&sph->getCenter()));
            objColor[i] = sf::Glsl::Vec3(c.r/255.f, c.g/255.f, c.b/255.f);
            objNormal[i] = sf::Glsl::Vec3(0.f,0.f,0.f);
        }
        else if (auto* pl = dynamic_cast<class Plane*>(o)) {
            objType[i] = 1.0f;
            objPos[i] = sf::Glsl::Vec3(static_cast<float>(pl->getPoint().getX()), static_cast<float>(pl->getPoint().getY()), static_cast<float>(pl->getPoint().getZ()));
            objRadius[i] = 0.0f;
            objNormal[i] = sf::Glsl::Vec3(static_cast<float>(pl->getNormal().getX()), static_cast<float>(pl->getNormal().getY()), static_cast<float>(pl->getNormal().getZ()));
            sf::Color c = pl->getColorAt(*const_cast<Vector3*>(&pl->getPoint()));
            objColor[i] = sf::Glsl::Vec3(c.r/255.f, c.g/255.f, c.b/255.f);
        } else {
            objType[i] = -1.0f;
            objPos[i] = sf::Glsl::Vec3(0,0,0);
            objRadius[i] = 0.0f;
            objColor[i] = sf::Glsl::Vec3(0,0,0);
            objNormal[i] = sf::Glsl::Vec3(0,0,0);
        }
    }

    // Set shader uniforms
    shader.setUniform("u_resolution", sf::Glsl::Vec2(static_cast<float>(width), static_cast<float>(height)));
    shader.setUniform("u_camOrigin", sf::Glsl::Vec3(static_cast<float>(camOrigin.getX()), static_cast<float>(camOrigin.getY()), static_cast<float>(camOrigin.getZ())));
    shader.setUniform("u_camForward", sf::Glsl::Vec3(static_cast<float>(camForward.getX()), static_cast<float>(camForward.getY()), static_cast<float>(camForward.getZ())));
    shader.setUniform("u_camRight", sf::Glsl::Vec3(static_cast<float>(camRight.getX()), static_cast<float>(camRight.getY()), static_cast<float>(camRight.getZ())));
    shader.setUniform("u_camUp", sf::Glsl::Vec3(static_cast<float>(camUp.getX()), static_cast<float>(camUp.getY()), static_cast<float>(camUp.getZ())));
    shader.setUniform("u_fov", static_cast<float>(fov));
    shader.setUniform("u_light", sf::Glsl::Vec3(static_cast<float>(light.getX()), static_cast<float>(light.getY()), static_cast<float>(light.getZ())));
    shader.setUniform("u_objCount", static_cast<int>(count));
    if (count > 0) {
        shader.setUniformArray("u_objPos", objPos.data(), count);
        shader.setUniformArray("u_objColor", objColor.data(), count);
        shader.setUniformArray("u_objNormal", objNormal.data(), count);
        shader.setUniformArray("u_objRadius", objRadius.data(), count);
        shader.setUniformArray("u_objType", objType.data(), count);
    }

    // Draw full-screen quad with shader
    sf::RectangleShape quad(sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
    quad.setPosition(sf::Vector2f(0.f, 0.f));
    window.draw(quad, &shader);
}


bool RayMarchingRender::ensureShaderLoaded() {
    if (shaderLoaded) return true;
    // Try load from project-relative shaders folder
    if (shader.loadFromFile("shaders/raymarch.frag", sf::Shader::Type::Fragment)) {
        shaderLoaded = true;
        return true;
    }
    // Try load from working dir
    if (shader.loadFromFile(std::string("./shaders/raymarch.frag"), sf::Shader::Type::Fragment)) {
        shaderLoaded = true;
        return true;
    }
    // failed
    return false;
}

