#ifndef QUATERNIONJULIA_H
#define QUATERNIONJULIA_H

#include "Object.h"
#include "../Vector3.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <string>

struct QuaternionJulia : public Object {
    Vector3 center;
    Vector3 c;  // Julia set constant (quaternion: w=0, xyz=this vector)
    int iterations;
    double bailout;
    double scale;
    sf::Color color;
    std::string texture;

public:
    QuaternionJulia(const Vector3& center, const Vector3& juliaC, int iter = 8, double s = 1.0, sf::Color col = sf::Color::Magenta, const std::string& tex = "")
        : center(center), c(juliaC), iterations(iter), bailout(2.0), scale(s), color(col), texture(tex) {}

    // Quaternion Julia set distance estimator
    // Formula: z = z^2 + c where z and c are quaternions
    double distanceToSurface(const Vector3& p) override {
        // Quick bounding sphere check
        double distFromCenter = (p - center).magnitude();
        double boundingRadius = scale * 2.0;
        if (distFromCenter > boundingRadius * 3.0) {
            return distFromCenter - boundingRadius;
        }
        
        // Transform point to Julia set space
        Vector3 z = (p - center) / scale;
        double dr = 1.0;  // Derivative accumulator
        
        // Iterate the quaternion Julia set formula: z = z^2 + c
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
            
            // Quaternion multiplication: z^2
            // For quaternion q = (w, x, y, z), q^2 = (w^2 - |v|^2, 2w*v + v×v)
            // For pure quaternion (w=0): z^2 = (-|v|^2, v×v) = (-(x^2+y^2+z^2), cross(v,v))
            // Since v×v = 0 for any vector, z^2 = (-|v|^2, 0) = -|v|^2
            // But we need to preserve the vector part, so we use:
            // z^2 = (x^2 - y^2 - z^2, 2xy, 2xz, 2yz) for quaternion (0, x, y, z)
            // Actually, for pure quaternions: (0,x,y,z)^2 = (-(x^2+y^2+z^2), 0, 0, 0)
            // But we want to keep it as a 3D vector, so we use the standard quaternion square:
            double x = z.getX();
            double y = z.getY();
            double zz = z.getZ();
            
            // Quaternion square for 3D vector (pure quaternion): z^2 = (x^2 - y^2 - z^2, 2xy, 2xz)
            // This is the standard formula for quaternion Julia sets in 3D
            Vector3 zSquared(
                x * x - y * y - zz * zz,
                2.0 * x * y,
                2.0 * x * zz
            );
            
            // Add Julia constant: z = z^2 + c
            z = zSquared + c;
            
            // Update derivative: dr = 2 * |z| * dr
            dr = 2.0 * r * dr + 1.0;
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
        
        // Handle negative distances
        if (distance < 0.0) {
            distance = 0.0005;
        }
        
        // Ensure minimum distance
        if (distance < 0.0001) {
            distance = 0.0001;
        }
        
        // Clamp to reasonable range
        if (!(distance == distance) || distance > 100.0) {
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
};

#endif
