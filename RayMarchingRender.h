//
// Created by Ilya Plisko on 05.11.25.
//
#pragma once
#include <SFML/Graphics.hpp>
#include "Constants.h"
#include "Ray.h"
#include "Vector3.h"
#include "Objects/Object.h"
#include "Angle.h"
#include <vector>
#include <map>
#include <string>


#ifndef TEST3D_SFMLRENDER_H
#define TEST3D_SFMLRENDER_H


struct RayMarchingRender {
    Vector3 light;
    unsigned width, height;
    double fov;
    sf::RenderWindow window;
    std::vector<Object*> objects;
    sf::Shader shader;
    bool shaderLoaded = false;
    std::vector<sf::Texture> textures;  // Store loaded textures
    std::map<std::string, unsigned> textureMap;  // Map texture path to texture index
    std::vector<int> objectTextureIndices;  // Map object index to texture index (-1 = no texture)
    unsigned numTexturesLoaded = 0;  // Track how many textures are actually loaded
    static constexpr unsigned MAX_OBJECTS = 32;


    RayMarchingRender(unsigned width, unsigned height, double fov, const Vector3& light, const std::vector<Object*>& objects) :
        width(width), height(height), fov(fov), objects(objects), light(light),
        window(sf::VideoMode({width, height}), "Presentation") {}

    RayMarchingRender(const short width, const short height, const double fov, const std::vector<Object*>& objects) :
        RayMarchingRender(width, height, fov, Z*-1, objects) {}
    void renderFrame(Ray);
    bool ensureShaderLoaded();
    void loadTexturesFromObjects();
    std::string getTexturePath(Object* obj);
    std::tuple<double, Vector3, Object&> intersection(const Vector3&, const Vector3&);
    std::pair<double, Object*> distanceToClosest(const Vector3&);

    void setWidth(unsigned newWidth) {
        width = newWidth;
        window.create(sf::VideoMode({width, height}), "Presentation");
    }

    void setHeight(unsigned newHeight) {
        height = newHeight;
        window.create(sf::VideoMode({width, height}), "Presentation");
    }

    void setSize(unsigned newWidth, unsigned newHeight) {
        width = newWidth;
        height = newHeight;
        window.create(sf::VideoMode({width, height}), "Presentation");
    }

};


#endif //TEST3D_SFMLRENDER_H