//
// Created by Ilya Plisko on 05.11.25.
//

#include "RayMarchingRender.h"
#include "Quaternion.h"
#include <vector>

#include "CameraBasis.h"
#include "Objects/Sphere.h"
#include "Objects/Plane.h"
#include "Objects/Box.h"
#include "Objects/Cylinder.h"
#include "Objects/Capsule.h"
#include "Objects/Torus.h"
#include "Objects/Mandelbulb.h"
#include "Objects/Terrain.h"
#include "Objects/QuaternionJulia.h"
#include "CSGoperations/Union.h"
#include "CSGoperations/Difference.h"
#include "CSGoperations/Intersection.h"
#include <map>
#include <algorithm>

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

    // Load textures from objects if not already loaded
    if (textures.empty()) {
        loadTexturesFromObjects();
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
    std::vector<float> objType(count);
    std::vector<sf::Glsl::Vec3> objNormal(count);
    std::vector<float> objTextureIndex(count);  // Use float for shader compatibility
    std::vector<float> objExtra(count);
    std::vector<float> objReflectivity(count);

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
        else if (dynamic_cast<Mandelbulb*>(o)) objType[i] = 9.0f;
        else if (dynamic_cast<Terrain*>(o)) objType[i] = 10.0f;
        else if (dynamic_cast<QuaternionJulia*>(o)) objType[i] = 11.0f;
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
            // CSG objects don't have textures
            objTextureIndex[i] = -1.0f;
        } else {
            // For primitives
            Vector3 center = o->getCenterOrPoint();
            objPos[i] = sf::Glsl::Vec3(static_cast<float>(center.getX()),
                                        static_cast<float>(center.getY()),
                                        static_cast<float>(center.getZ()));

            objRadius[i] = o->getRadiusOrSize();
            objRadius2[i] = 0.0f;

            if (objType[i] == 4.0f) { // capsule
                objRadius2[i] = dynamic_cast<Capsule*>(o)->getHeight();
            } else if (objType[i] == 5.0f) { // torus
                objRadius[i] = dynamic_cast<Torus*>(o)->getMajorRadius();
                objRadius2[i] = dynamic_cast<Torus*>(o)->getMinorRadius();
            } else if (objType[i] == 9.0f) { // mandelbulb
                Mandelbulb* mb = dynamic_cast<Mandelbulb*>(o);
                objRadius[i] = static_cast<float>(mb->scale);
                objRadius2[i] = static_cast<float>(mb->power);
                // Store iterations in objNormal.x (we'll extract it in shader)
                // Don't overwrite this - Mandelbulb doesn't use getNormalAtOrigin()
                objNormal[i] = sf::Glsl::Vec3(static_cast<float>(mb->iterations), 0.0f, 0.0f);
            } else if (objType[i] == 10.0f) { // terrain
                // Pack terrain parameters using existing arrays to avoid new uniforms
                // u_objPos = (origin.x, seed, origin.z)
                // u_objRadius = amplitude
                // u_objRadius2 = base frequency
                // u_objNormal = (octaves, lacunarity, gain)
                // u_objColor2 = (warpStrength, ridgedToggle, warpToggle)
                Terrain* t = dynamic_cast<Terrain*>(o);
                // Center already set from getCenterOrPoint(): (origin.x, seed, origin.z)
                objRadius[i] = t->getRadiusOrSize();
                objRadius2[i] = t->getFrequency();
                // getNormalAtOrigin encodes (octaves, lacunarity, gain)
                Vector3 ng = t->getNormalAtOrigin();
                objNormal[i] = sf::Glsl::Vec3(static_cast<float>(ng.getX()), static_cast<float>(ng.getY()), static_cast<float>(ng.getZ()));
                objColor2[i] = sf::Glsl::Vec3(t->getWarpStrength(), t->isRidged() ? 1.0f : 0.0f, t->isWarpEnabled() ? 1.0f : 0.0f);
                objExtra[i] = t->originXZ.getZ();
            } else if (objType[i] == 11.0f) { // quaternion julia
                QuaternionJulia* qj = dynamic_cast<QuaternionJulia*>(o);
                objRadius[i] = static_cast<float>(qj->scale);
                objRadius2[i] = 0.0f;
                // Store Julia constant c in objNormal, iterations in objNormal.x
                Vector3 juliaC = qj->c;
                objNormal[i] = sf::Glsl::Vec3(static_cast<float>(qj->iterations),
                                             static_cast<float>(juliaC.getX()),
                                             static_cast<float>(juliaC.getY()));
                // Store z component of c in objRadius2 (we'll use it in shader)
                objRadius2[i] = static_cast<float>(juliaC.getZ());
            } else {
                // For other objects, set normal from getNormalAtOrigin()
                Vector3 n = o->getNormalAtOrigin();
                objNormal[i] = sf::Glsl::Vec3(static_cast<float>(n.getX()),
                                            static_cast<float>(n.getY()),
                                            static_cast<float>(n.getZ()));
            }

            sf::Color c = o->getColorAtOrigin();
            objColor[i] = sf::Glsl::Vec3(c.r / 255.f, c.g / 255.f, c.b / 255.f);
            // Do not override objColor2 for terrain (used to pack warp/ridged toggles)
            if (objType[i] != 10.0f) {
                objColor2[i] = objColor[i]; // same for primitives
            }

            // Get texture index for this object
            std::string texPath = getTexturePath(o);
            if (!texPath.empty() && textureMap.find(texPath) != textureMap.end()) {
                objTextureIndex[i] = static_cast<float>(textureMap[texPath]);
            } else {
                objTextureIndex[i] = -1.0f; // No texture
            }
        }

        // Get reflectivity for all objects
        objReflectivity[i] = o->getReflectivity();
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
        shader.setUniformArray("u_objColor2", objColor2.data(), count);
        shader.setUniformArray("u_objNormal", objNormal.data(), count);
        shader.setUniformArray("u_objRadius", objRadius.data(), count);
        shader.setUniformArray("u_objRadius2", objRadius2.data(), count);
        shader.setUniformArray("u_objType", objType.data(), count);
        shader.setUniformArray("u_objTextureIndex", objTextureIndex.data(), count);
        shader.setUniformArray("u_objExtra", objExtra.data(), count);
        shader.setUniformArray("u_objReflectivity", objReflectivity.data(), count);
    } else {
        // Set empty arrays to avoid shader errors
        std::vector<sf::Glsl::Vec3> emptyVec3(1);
        std::vector<float> emptyFloat(1, 0.0f);
        shader.setUniformArray("u_objPos", emptyVec3.data(), 1);
        shader.setUniformArray("u_objColor", emptyVec3.data(), 1);
        shader.setUniformArray("u_objColor2", emptyVec3.data(), 1);
        shader.setUniformArray("u_objNormal", emptyVec3.data(), 1);
        shader.setUniformArray("u_objRadius", emptyFloat.data(), 1);
        shader.setUniformArray("u_objRadius2", emptyFloat.data(), 1);
        shader.setUniformArray("u_objType", emptyFloat.data(), 1);
        shader.setUniformArray("u_objReflectivity", emptyFloat.data(), 1);
    }
    
    // Set textures to individual shader uniforms (GLSL doesn't support dynamic sampler array indexing)
    // Only set textures that were successfully loaded
    if (numTexturesLoaded > 0) shader.setUniform("u_texture0", textures[0]);
    if (numTexturesLoaded > 1) shader.setUniform("u_texture1", textures[1]);
    if (numTexturesLoaded > 2) shader.setUniform("u_texture2", textures[2]);
    if (numTexturesLoaded > 3) shader.setUniform("u_texture3", textures[3]);
    if (numTexturesLoaded > 4) shader.setUniform("u_texture4", textures[4]);
    if (numTexturesLoaded > 5) shader.setUniform("u_texture5", textures[5]);
    if (numTexturesLoaded > 6) shader.setUniform("u_texture6", textures[6]);
    if (numTexturesLoaded > 7) shader.setUniform("u_texture7", textures[7]);

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
    // failed - print error
    std::cerr << "ERROR: Failed to load shader!" << std::endl;
    return false;
}

std::string RayMarchingRender::getTexturePath(Object* obj) {
    if (auto* box = dynamic_cast<Box*>(obj)) {
        return box->texture;
    } else if (auto* sphere = dynamic_cast<Sphere*>(obj)) {
        return sphere->texture;
    } else if (auto* mandelbulb = dynamic_cast<Mandelbulb*>(obj)) {
        return mandelbulb->texture;
    } else if (auto* quaternionJulia = dynamic_cast<QuaternionJulia*>(obj)) {
        return quaternionJulia->texture;
    }
    return "";
}

void RayMarchingRender::loadTexturesFromObjects() {
    textures.clear();
    textureMap.clear();
    objectTextureIndices.clear();

    // Collect all unique texture paths
    std::map<std::string, unsigned> pathToIndex;
    unsigned nextIndex = 0;

    for (Object* obj : objects) {
        std::string texPath = getTexturePath(obj);
        if (!texPath.empty() && pathToIndex.find(texPath) == pathToIndex.end()) {
            pathToIndex[texPath] = nextIndex++;
        }
    }

    // Load textures
    textures.resize(pathToIndex.size());
    for (const auto& pair : pathToIndex) {
        const std::string& path = pair.first;
        unsigned index = pair.second;

        // Try different path variations
        std::vector<std::string> paths = {
            path,
            "../" + path,
            "./" + path
        };

        bool loaded = false;
        for (const auto& tryPath : paths) {
            if (textures[index].loadFromFile(tryPath)) {
                textures[index].setRepeated(true);
                textureMap[path] = index;
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            // If loading failed, remove from map
            textureMap.erase(path);
        } else {
            numTexturesLoaded = std::max(numTexturesLoaded, index + 1);
        }
    }
}
