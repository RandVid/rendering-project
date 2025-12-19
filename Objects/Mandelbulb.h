#ifndef MANDELBULB_H
#define MANDELBULB_H

#include "Object.h"
#include "../Vector3.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <string>

struct Mandelbulb : public Object {
    Vector3 center;
    int iterations;
    double power;
    double bailout;
    double scale;
    sf::Color color;
    std::string texture;
    float reflectivity = 0.0f; // 0 = not reflective, 1 = mirror

public:
    Mandelbulb(const Vector3& c, int iter = 8, double p = 8.0, sf::Color col = sf::Color::Cyan, double s = 1.0)
        : center(c), iterations(iter), power(p), bailout(2.0), scale(s), color(col) {}
    Mandelbulb(const Vector3& c, int iter, double p, sf::Color col, double s, float refl)
        : center(c), iterations(iter), power(p), bailout(2.0), scale(s), color(col), reflectivity(refl) {}
    Mandelbulb(const Vector3& c, int iter = 8, double p = 8.0, sf::Color col = sf::Color::Cyan, double s = 1.0, const std::string& tex = "")
        : center(c), iterations(iter), power(p), bailout(2.0), scale(s), color(col), texture(tex) {}

    // Mandelbulb distance estimator
    // Formula: z = z^n + c where z starts at origin
    double distanceToSurface(const Vector3& p) override {
        // Quick bounding sphere check - if very far, return large distance
        double distFromCenter = (p - center).magnitude();
        double boundingRadius = scale * 3.0;  // Approximate bounding radius
        // Only use bounding sphere for very far points to avoid interfering with close rendering
        if (distFromCenter > boundingRadius * 3.0) {
            return distFromCenter - boundingRadius;  // Distance to bounding sphere
        }
        
        // Transform point to Mandelbulb space
        Vector3 c = (p - center) / scale;
        Vector3 z(0, 0, 0);  // Start at origin
        double dr = 1.0;     // Derivative accumulator
        
        // Iterate the Mandelbulb formula
        for (int i = 0; i < iterations; i++) {
            double r = z.magnitude();
            
            // Early exit if escaped
            if (r > bailout) {
                break;
            }
            
            // Avoid division by zero
            if (r < 1e-10) {
                r = 1e-10;
                z = Vector3(1e-10, 0, 0);
            }
            
            // Convert to spherical coordinates
            double zr_ratio = z.getZ() / r;
            zr_ratio = std::max(-1.0, std::min(1.0, zr_ratio));
            double theta = std::acos(zr_ratio);
            double phi = std::atan2(z.getY(), z.getX());
            
            // Update derivative: dr = n * r^(n-1) * dr + 1
            dr = std::pow(r, power - 1.0) * power * dr + 1.0;
            
            // Raise to power in spherical coordinates: (r, theta, phi) -> (r^n, n*theta, n*phi)
            double zr = std::pow(r, power);
            theta = theta * power;
            phi = phi * power;
            
            // Convert back to cartesian
            z = Vector3(
                std::sin(theta) * std::cos(phi),
                std::sin(theta) * std::sin(phi),
                std::cos(theta)
            ) * zr;
            
            // Add constant: z = z^n + c
            z = z + c;
        }
        
        // Calculate final magnitude
        double r = z.magnitude();
        
        // Ensure reasonable values
        if (r < 1e-10) r = 1e-10;
        if (dr < 1e-10) dr = 1e-10;
        
        // Distance estimator: 0.5 * log(r) * r / dr
        double distance = 0.5 * std::log(r) * r / dr;
        
        // Scale the distance
        distance = distance * scale;
        
        // Handle negative distances (inside set) - use very small positive value
        // Make it smaller than EPS (0.001) to ensure proper hits
        if (distance < 0.0) {
            distance = 0.0005;  // Smaller than EPS to ensure hit detection
        }
        
        // Ensure minimum distance is reasonable but not too large
        // This helps with ray marching convergence
        if (distance < 0.0001) {
            distance = 0.0001;
        }
        
        // Clamp to reasonable range
        if (!(distance == distance) || distance > 100.0) {  // Check for NaN and clamp
            distance = 100.0;
        }
        
        return distance;
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
        return color;
    }

    Vector3 getCenterOrPoint() const override { return center; }
    float getRadiusOrSize() const override { return static_cast<float>(scale * 2.0); }
    sf::Color getColorAtOrigin() const override { return color; }
    Vector3 getNormalAtOrigin() const override { return Vector3(0, 1, 0); }
    float getReflectivity() const override { return reflectivity; }
};

#endif
