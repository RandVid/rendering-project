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
    static constexpr unsigned MAX_OBJECTS = 128;


    RayMarchingRender(unsigned width, unsigned height, double fov, const Vector3& light, const std::vector<Object*>& objects) :
        width(width), height(height), fov(fov), objects(objects), light(light),
        window(sf::VideoMode({width, height}), "Penis") {}

    RayMarchingRender(const short width, const short height, const double fov, const std::vector<Object*>& objects) :
        RayMarchingRender(width, height, fov, Z*-1, objects) {}
    void renderFrame(Ray);
    bool ensureShaderLoaded();
    std::tuple<double, Vector3, Object&> intersection(const Vector3&, const Vector3&);
    std::pair<double, Object*> distanceToClosest(Vector3&);

    void setWidth(unsigned newWidth) {
        width = newWidth;
        window.create(sf::VideoMode({width, height}), "bigger PENIS");
    }

    void setHeight(unsigned newHeight) {
        height = newHeight;
        window.create(sf::VideoMode({width, height}), "smaller penis");
    }

    void setSize(unsigned newWidth, unsigned newHeight) {
        width = newWidth;
        height = newHeight;
        window.create(sf::VideoMode({width, height}), "different size penis");
    }

};


#endif //TEST3D_SFMLRENDER_H