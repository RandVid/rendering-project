#ifndef RENDERING_PROJECT_TERRAIN_H
#define RENDERING_PROJECT_TERRAIN_H

#include "Object.h"
#include "../Vector3.h"
#include <SFML/Graphics/Color.hpp>
#include <cmath>
#include <functional>

// Procedural heightfield terrain. Distance estimator compatible with sphere tracing:
// d(p) = p.z - height(p.xy)   // Note: this project uses Z as up-axis
// Parameters are seed-driven and deterministic. We also keep a CPU-side
// implementation for parity with the GPU shader, though the renderer primarily
// ray-marches on GPU.
struct Terrain : public Object {
    // World-space horizontal offset for the heightfield domain (XY). Z is up in this project.
    Vector3 originXZ; // use x,y as horizontal; z is ignored here

    // Visual parameters
    sf::Color color;

    // Noise/FBM parameters
    float amplitude = 3.0f;    // height scale
    float frequency = 0.2f;    // base frequency (1/units)
    float seed = 0.0f;         // deterministic seed
    int   octaves = 5;         // FBM octaves
    float lacunarity = 2.0f;   // frequency multiplier per octave
    float gain = 0.5f;         // amplitude multiplier per octave

    // Optional modes
    float warpStrength = 0.0f; // domain warp amount (0 disables)
    bool  ridged = false;      // ridged FBM toggle
    bool  warp = false;        // domain warp toggle

    // Constructors mirror style of other objects
    Terrain(
        const Vector3& originXZ,
        float amplitude,
        float frequency,
        float seed,
        int octaves,
        float lacunarity,
        float gain,
        sf::Color color = sf::Color(180, 170, 160)
    ) : originXZ(originXZ), color(color), amplitude(amplitude), frequency(frequency), seed(seed),
        octaves(octaves), lacunarity(lacunarity), gain(gain) {}

    Terrain(
        const Vector3& originXZ,
        float amplitude,
        float frequency,
        float seed,
        sf::Color color = sf::Color(180, 170, 160)
    ) : originXZ(originXZ), color(color), amplitude(amplitude), frequency(frequency), seed(seed) {}

    // Domain warp / ridged configuration helpers
    Terrain& setWarp(float strength, bool enabled=true) { warpStrength = strength; warp = enabled; return *this; }
    Terrain& setRidged(bool enabled) { ridged = enabled; return *this; }

    // CPU distance estimator (kept relatively light and deterministic)
    double distanceToSurface(const Vector3& p) override {
        // Z-up: d(p) = p.z - height(p.x, p.y)
        return p.getZ() - heightAt(p.getX(), p.getY());
    }

    Vector3 getNormalAt(const Vector3& p) override {
        // Finite differences consistent with GLSL epsilon scale
        const double e = 1e-3;
        double hx = heightAt(p.getX() + e, p.getY()) - heightAt(p.getX() - e, p.getY());
        double hy = heightAt(p.getX(), p.getY() + e) - heightAt(p.getX(), p.getY() - e);
        // For d(p) = z - h(x,y), gradient is ( -dh/dx, -dh/dy, 1 )
        return Vector3(-hx * 0.5, -hy * 0.5, 1.0).normalized(); // scale halves to keep magnitude stable
    }

    sf::Color getColorAt(const Vector3&) override { return color; }

    // Upload helpers (match existing patterns)
    Vector3 getCenterOrPoint() const override { return Vector3(originXZ.getX(), seed, originXZ.getZ()); }
    float getRadiusOrSize() const override { return amplitude; }
    sf::Color getColorAtOrigin() const override { return color; }
    Vector3 getNormalAtOrigin() const override { return Vector3((double)octaves, lacunarity, gain); }

    // Custom getters for renderer packing
    float getFrequency() const { return frequency; }
    float getWarpStrength() const { return warpStrength; }
    bool  isRidged() const { return ridged; }
    bool  isWarpEnabled() const { return warp; }

    // Public hooks for shading systems
    float heightAtXZ(double x, double z) const { return heightAt(x, z); }
    float heightAtPoint(const Vector3& p) const { return heightAt(p.getX(), p.getZ()); }
    float slopeFactorAt(const Vector3& p) {
        Vector3 n = getNormalAt(p);
        return 1.0f - static_cast<float>(std::max(0.0, std::min(1.0, n.getZ() == 0 && n.getX() == 0 && n.getY() == 0 ? 0.0 : n.getY())));
    }

private:
    // Simple 2D value noise with smooth interpolation, seeded
    static inline float hash(float x) {
        // Keep deterministic and cheap
        float s = std::sin(x) * 43758.5453f;
        return s - std::floor(s);
    }

    static inline float hash2(float x, float y) { return hash(x * 127.1f + y * 311.7f); }

    float valueNoise2D(float x, float y) const {
        float sx = std::floor(x);
        float sy = std::floor(y);
        float fx = x - sx;
        float fy = y - sy;

        // incorporate seed by offsetting lattice space
        float ox = sx + std::floor(seed * 53.0f);
        float oy = sy + std::floor(seed * 91.0f);

        float v00 = hash2(ox + 0.0f, oy + 0.0f);
        float v10 = hash2(ox + 1.0f, oy + 0.0f);
        float v01 = hash2(ox + 0.0f, oy + 1.0f);
        float v11 = hash2(ox + 1.0f, oy + 1.0f);

        auto smooth = [](float t){ return t*t*(3.0f - 2.0f*t); };
        float tx = smooth(fx);
        float ty = smooth(fy);
        float a = v00 + (v10 - v00) * tx;
        float b = v01 + (v11 - v01) * ty;
        return a + (b - a) * ty;
    }
    float fbm2D(float x, float y) const {
        float amp = 1.0f;
        float freq = frequency;
        float sum = 0.0f;
        int oct = std::max(1, std::min(8, octaves)); // clamp for perf/stability

        float px = x;
        float py = y;

        if (warp && warpStrength > 0.0f) {
            // light domain warp using lower-frequency noise to avoid aliasing
            float wf = std::max(0.01f, frequency * 0.5f);
            float wx = valueNoise2D(x * wf + 13.1f * seed, y * wf + 37.7f * seed);
            float wy = valueNoise2D(x * wf + 91.4f * seed + 17.0f, y * wf + 27.9f * seed + 11.0f);
            px += (wx * 2.0f - 1.0f) * warpStrength;
            py += (wy * 2.0f - 1.0f) * warpStrength;
        }

        for (int i = 0; i < oct; ++i) {
            float n = valueNoise2D(px * freq + 17.0f * seed, py * freq + 29.0f * seed);
            if (ridged) {
                // ridged: sharpen peaks
                n = 1.0f - std::fabs(2.0f * n - 1.0f);
            }
            sum += n * amp;
            freq *= lacunarity;
            amp *= gain;
        }
        return sum;
    }

    float heightAt(double x, double z) const {
        // World-space continuity: we always evaluate in world coordinates minus origin offset
        float px = static_cast<float>(x - originXZ.getX());
        float pz = static_cast<float>(z - originXZ.getZ());
        return amplitude * fbm2D(px, pz);
    }
};

#endif // RENDERING_PROJECT_TERRAIN_H
