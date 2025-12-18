#ifndef MANDELBULB_H
#define MANDELBULB_H

#include "Object.h"
#include "../Vector3.h"
#include <SFML/Graphics.hpp>
#include <cmath>

struct Mandelbulb : public Object {
    Vector3 center;
    int iterations;
    double power;
    double bailout;
    double scale;
    sf::Color color;

public:
    Mandelbulb(const Vector3& c, int iter = 8, double p = 8.0, sf::Color col = sf::Color::Cyan, double s = 1.0)
        : center(c), iterations(iter), power(p), bailout(2.0), scale(s), color(col) {}

    // Mandelbulb distance estimator using sphere tracing
    double distanceToSurface(const Vector3& p) override {
        Vector3 pos = (p - center) / scale;
        Vector3 z = pos;
        double dr = 1.0;
        double r = 0.0;
        
        for (int i = 0; i < iterations; i++) {
            r = z.magnitude();
            if (r > bailout) break;
            
            // Avoid division by zero
            if (r < 1e-10) r = 1e-10;
            
            // Convert to polar coordinates
            double theta = std::acos(z.getZ() / r);
            double phi = std::atan2(z.getY(), z.getX());
            dr = std::pow(r, power - 1.0) * power * dr + 1.0;
            
            // Scale and rotate the point
            double zr = std::pow(r, power);
            theta = theta * power;
            phi = phi * power;
            
            // Convert back to cartesian coordinates
            z = Vector3(
                std::sin(theta) * std::cos(phi),
                std::sin(theta) * std::sin(phi),
                std::cos(theta)
            ) * zr;
            
            z = z + pos;
        }
        
        if (dr < 1e-10) dr = 1e-10;
        return 0.5 * std::log(r) * r / dr * scale;
    }

    Vector3 getNormalAt(const Vector3& p) override {
        const double e = 1e-4;
        return Vector3(
            distanceToSurface(p + Vector3(e, 0, 0)) - distanceToSurface(p - Vector3(e, 0, 0)),
            distanceToSurface(p + Vector3(0, e, 0)) - distanceToSurface(p - Vector3(0, e, 0)),
            distanceToSurface(p + Vector3(0, 0, e)) - distanceToSurface(p - Vector3(0, 0, e))
        ).normalized();
    }

    sf::Color getColorAt(const Vector3& p) override {
        // Optional: Add color variation based on distance or iteration count
        return color;
    }

    Vector3 getCenterOrPoint() const override { return center; }
    float getRadiusOrSize() const override { return 2.0f; }
    sf::Color getColorAtOrigin() const override { return color; }
    Vector3 getNormalAtOrigin() const override { return Vector3(0, 1, 0); }
};

#endif

