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
#include "Objects/Box.h"
#include "Objects/Cylinder.h"
#include "Objects/Capsule.h"
#include "Objects/Torus.h"
#include "CSGoperations/Union.h"
#include "CSGoperations/Difference.h"
#include "CSGoperations/Intersection.h"

std::pair<double, Object*> RayMarchingRender::distanceToClosest(const Vector3& p) {
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
    std::vector<float> objRadius2(count);
    std::vector<sf::Glsl::Vec3> objColor(count);
    std::vector<sf::Glsl::Vec3> objColor2(count);
    std::vector<sf::Glsl::Vec3> objSize(count);
    std::vector<float> objType(count);
    std::vector<sf::Glsl::Vec3> objNormal(count);

    for (unsigned i = 0; i < count; ++i) {
        Object* o = objects[i];

        // Assign objType based on dynamic cast
        if (dynamic_cast<Sphere*>(o)) objType[i] = 0.0f;
        else if (dynamic_cast<Plane*>(o)) objType[i] = 1.0f;
        else if (dynamic_cast<Box*>(o)) objType[i] = 2.0f;
        else if (dynamic_cast<Cylinder*>(o)) objType[i] = 3.0f;
        else if (dynamic_cast<Capsule*>(o)) objType[i] = 4.0f;
        else if (dynamic_cast<Torus*>(o)) objType[i] = 5.0f;
        else if (dynamic_cast<Union*>(o)) objType[i] = 6.0f;
        else if (dynamic_cast<Intersection*>(o)) objType[i] = 7.0f;
        else if (dynamic_cast<Difference*>(o)) objType[i] = 8.0f;
        else objType[i] = -1.0f;

        // For primitives and CSG, set data
        if (objType[i] >= 6.0f && objType[i] <= 8.0f) { // CSG: Union(6), Intersection(7), Difference(8)
            // Assume Union/Intersection/Difference of two spheres
            Object* childA = nullptr;
            Object* childB = nullptr;
            if (auto* un = dynamic_cast<Union*>(o)) {
                childA = un->getA();
                childB = un->getB();
            } else if (auto* inter = dynamic_cast<Intersection*>(o)) {
                childA = inter->getA();
                childB = inter->getB();
            } else if (auto* diff = dynamic_cast<Difference*>(o)) {
                childA = diff->getA();
                childB = diff->getB();
            }
            if (childA && childB) {
                auto* sphA = dynamic_cast<Sphere*>(childA);
                auto* sphB = dynamic_cast<Sphere*>(childB);
                if (sphA && sphB) {
                    objPos[i] = sf::Glsl::Vec3(static_cast<float>(sphA->getCenter().getX()),
                                               static_cast<float>(sphA->getCenter().getY()),
                                               static_cast<float>(sphA->getCenter().getZ()));
                    objRadius[i] = static_cast<float>(sphA->getRadius());
                    objNormal[i] = sf::Glsl::Vec3(static_cast<float>(sphB->getCenter().getX()),
                                                  static_cast<float>(sphB->getCenter().getY()),
                                                  static_cast<float>(sphB->getCenter().getZ()));
                    objRadius2[i] = static_cast<float>(sphB->getRadius());
                    sf::Color cA = sphA->getColorAtOrigin();
                    sf::Color cB = sphB->getColorAtOrigin();
                    objColor[i] = sf::Glsl::Vec3(cA.r / 255.f, cA.g / 255.f, cA.b / 255.f);
                    objColor2[i] = sf::Glsl::Vec3(cB.r / 255.f, cB.g / 255.f, cB.b / 255.f);
                }
            }
        } else {
            // For primitives
            Vector3 center = o->getCenterOrPoint();
            objPos[i] = sf::Glsl::Vec3(static_cast<float>(center.getX()),
                                        static_cast<float>(center.getY()),
                                        static_cast<float>(center.getZ()));

            objRadius[i] = o->getRadiusOrSize();
            objRadius2[i] = 0.0f;

            if (objType[i] == 2.0f) {
                auto size = dynamic_cast<Box*>(o)->getSize();
                objSize[i] = {static_cast<float>(size.getX()), static_cast<float>(size.getY()), static_cast<float>(size.getZ())};
            }
            else if (objType[i] == 3.0f) {
                objRadius2[i] = dynamic_cast<Cylinder*>(o)->getHeight();
            }
            else if (objType[i] == 4.0f) { // capsule
                objRadius2[i] = dynamic_cast<Capsule*>(o)->getHeight();
            } else if (objType[i] == 5.0f) { // torus
                objRadius[i] = dynamic_cast<Torus*>(o)->getMajorRadius();
                objRadius2[i] = dynamic_cast<Torus*>(o)->getMinorRadius();
            }

            sf::Color c = o->getColorAtOrigin();
            objColor[i] = sf::Glsl::Vec3(c.r / 255.f, c.g / 255.f, c.b / 255.f);
            objColor2[i] = objColor[i]; // same for primitives

            Vector3 n = o->getNormalAtOrigin();
            objNormal[i] = sf::Glsl::Vec3(static_cast<float>(n.getX()),
                                        static_cast<float>(n.getY()),
                                        static_cast<float>(n.getZ()));
        }
    }

    // for (unsigned i = 0; i < count; ++i) {
    //     Object* o = objects[i];
    //     if (auto* sph = dynamic_cast<class Sphere*>(o)) {
    //         objType[i] = 0.0f;
    //         objPos[i] = sf::Glsl::Vec3(static_cast<float>(sph->getCenter().getX()), static_cast<float>(sph->getCenter().getY()), static_cast<float>(sph->getCenter().getZ()));
    //         objRadius[i] = static_cast<float>(sph->getRadius());
    //         sf::Color c = sph->getColorAt(*const_cast<Vector3*>(&sph->getCenter()));
    //         objColor[i] = sf::Glsl::Vec3(c.r/255.f, c.g/255.f, c.b/255.f);
    //         objNormal[i] = sf::Glsl::Vec3(0.f,0.f,0.f);
    //     }
    //     else if (auto* pl = dynamic_cast<class Plane*>(o)) {
    //         objType[i] = 1.0f;
    //         objPos[i] = sf::Glsl::Vec3(static_cast<float>(pl->getPoint().getX()), static_cast<float>(pl->getPoint().getY()), static_cast<float>(pl->getPoint().getZ()));
    //         objRadius[i] = 0.0f;
    //         objNormal[i] = sf::Glsl::Vec3(static_cast<float>(pl->getNormal().getX()), static_cast<float>(pl->getNormal().getY()), static_cast<float>(pl->getNormal().getZ()));
    //         sf::Color c = pl->getColorAt(*const_cast<Vector3*>(&pl->getPoint()));
    //         objColor[i] = sf::Glsl::Vec3(c.r/255.f, c.g/255.f, c.b/255.f);
    //     } else {
    //         objType[i] = -1.0f;
    //         objPos[i] = sf::Glsl::Vec3(0,0,0);
    //         objRadius[i] = 0.0f;
    //         objColor[i] = sf::Glsl::Vec3(0,0,0);
    //         objNormal[i] = sf::Glsl::Vec3(0,0,0);
    //     }
    // }

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
        shader.setUniformArray("u_objSize", objSize.data(), count);
        shader.setUniformArray("u_objColor2", objColor2.data(), count);
        shader.setUniformArray("u_objNormal", objNormal.data(), count);
        shader.setUniformArray("u_objRadius", objRadius.data(), count);
        shader.setUniformArray("u_objRadius2", objRadius2.data(), count);
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
    if (shader.loadFromFile("../shaders/raymarch.frag", sf::Shader::Type::Fragment)) {
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

