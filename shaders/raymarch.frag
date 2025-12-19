uniform vec2 u_resolution;
uniform vec3 u_camOrigin;
uniform vec3 u_camForward;
uniform vec3 u_camRight;
uniform vec3 u_camUp;
uniform float u_fov;
uniform vec3 u_light;
uniform int u_objCount;

const int MAX_OBJECTS = 32;
uniform vec3 u_objPos[MAX_OBJECTS];
uniform vec3 u_objColor[MAX_OBJECTS];
uniform vec3 u_objColor2[MAX_OBJECTS];
uniform vec3 u_objNormal[MAX_OBJECTS];
uniform float u_objRadius[MAX_OBJECTS];
uniform float u_objRadius2[MAX_OBJECTS];
uniform float u_objType[MAX_OBJECTS];
uniform float u_objTextureIndex[MAX_OBJECTS];
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform sampler2D u_texture5;
uniform sampler2D u_texture6;
uniform sampler2D u_texture7;
uniform float u_objExtra[MAX_OBJECTS];
uniform float u_objReflectivity[MAX_OBJECTS];

// Reflection depth (0 = no reflections)
const int MAX_REFLECTION_DEPTH = 2;

// Reflection tuning
const float REFLECTION_BIAS = 0.02;
// Artistic boost for reflected contribution (not physically correct, but avoids "too dark" look)
const float REFLECTION_STRENGTH = 0.9;

vec3 skyColor() {
    return vec3(0.5, 0.7, 1.0);
}

vec4 FragColor;

// ------------------------
// Basic SDF functions
// ------------------------
float sphereSDF(vec3 p, vec3 center, float radius) {
    return length(p - center) - radius;
}

float planeSDF(vec3 p, vec3 point, vec3 normal) {
    return dot(p - point, normal);
}

float boxSDF(vec3 p, vec3 center, vec3 size) {
    vec3 d = abs(p - center) - size;
    return length(max(d, 0.0)) + min(max(d.x,max(d.y,d.z)),0.0);
}

float cylinderSDF(vec3 p, vec3 center, float radius, float height) {
    vec2 d = vec2(length(p.xz - center.xz) - radius, abs(p.y - center.y) - height*0.5);
    return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

float capsuleSDF(vec3 p, vec3 center, float radius, float height) {
    vec3 a = center - vec3(0, height*0.5, 0);
    vec3 b = center + vec3(0, height*0.5, 0);
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa,ba)/dot(ba,ba), 0.0, 1.0);
    return length(pa - ba*h) - radius;
}

float torusSDF(vec3 p, vec3 center, float R, float r) {
    vec2 q = vec2(length(p.xz - center.xz) - R, p.y - center.y);
    return length(q) - r;
}

float mandelbulbSDF(vec3 p, vec3 center, float scale, float power, float iterations) {
    // Quick bounding sphere check - if very far, return large distance
    float distFromCenter = length(p - center);
    float boundingRadius = scale * 3.0;  // Approximate bounding radius
    // Only use bounding sphere for very far points to avoid interfering with close rendering
    if (distFromCenter > boundingRadius * 3.0) {
        return distFromCenter - boundingRadius;  // Distance to bounding sphere
    }

    // Transform point to Mandelbulb space
    vec3 c = (p - center) / scale;
    vec3 z = vec3(0.0);  // Start at origin
    float dr = 1.0;      // Derivative accumulator
    float bailout = 2.0;

    // Iterate the Mandelbulb formula
    for (float i = 0.0; i < iterations; i += 1.0) {
        float r = length(z);

        // Early exit if escaped
        if (r > bailout) {
            break;
        }

        // Avoid division by zero
        if (r < 1e-10) {
            r = 1e-10;
            z = vec3(1e-10, 0.0, 0.0);
        }

        // Convert to spherical coordinates
        float zr_ratio = clamp(z.z / r, -1.0, 1.0);
        float theta = acos(zr_ratio);
        float phi = atan(z.y, z.x);

        // Update derivative: dr = n * r^(n-1) * dr + 1
        dr = pow(r, power - 1.0) * power * dr + 1.0;

        // Raise to power in spherical coordinates: (r, theta, phi) -> (r^n, n*theta, n*phi)
        float zr = pow(r, power);
        theta = theta * power;
        phi = phi * power;

        // Convert back to cartesian
        z = vec3(
        sin(theta) * cos(phi),
        sin(theta) * sin(phi),
        cos(theta)
        ) * zr;

        // Add constant: z = z^n + c
        z = z + c;
    }

    // Calculate final magnitude
    float r = length(z);

    // Ensure reasonable values
    if (r < 1e-10) r = 1e-10;
    if (dr < 1e-10) dr = 1e-10;

    // Distance estimator: 0.5 * log(r) * r / dr
    float distance = 0.5 * log(r) * r / dr;

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
    if (distance != distance || distance > 100.0) {
        distance = 100.0;
    }

    return distance;
}

float quaternionJuliaSDF(vec3 p, vec3 center, float scale, vec3 juliaC, float iterations) {
    // Quick bounding sphere check
    float distFromCenter = length(p - center);
    float boundingRadius = scale * 2.0;
    if (distFromCenter > boundingRadius * 3.0) {
        return distFromCenter - boundingRadius;
    }

    // Transform point to Julia set space
    vec3 z = (p - center) / scale;
    float dr = 1.0;
    float bailout = 2.0;

    // Iterate the quaternion Julia set formula: z = z^2 + c
    for (float i = 0.0; i < iterations; i += 1.0) {
        float r = length(z);

        // Early exit if escaped
        if (r > bailout) {
            break;
        }

        // Avoid division by zero
        if (r < 1e-10) {
            r = 1e-10;
            z = vec3(1e-10, 0.0, 0.0);
        }

        // Quaternion square for 3D vector: z^2 = (x^2 - y^2 - z^2, 2xy, 2xz)
        float x = z.x;
        float y = z.y;
        float zz = z.z;

        vec3 zSquared = vec3(
            x * x - y * y - zz * zz,
            2.0 * x * y,
            2.0 * x * zz
        );

        // Add Julia constant: z = z^2 + c
        z = zSquared + juliaC;

        // Update derivative: dr = 2 * |z| * dr
        dr = 2.0 * r * dr + 1.0;
    }

    // Calculate final magnitude
    float r = length(z);

    // Ensure reasonable values
    if (r < 1e-10) r = 1e-10;
    if (dr < 1e-10) dr = 1e-10;

    // Distance estimator: 0.5 * log(r) * r / dr
    float distance = 0.5 * log(r) * r / dr;

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
    if (distance != distance || distance > 100.0) {
        distance = 100.0;
    }

    return distance;
}

// Example: CSG operations
float opUnion(float d1, float d2) { return min(d1, d2); }
float opIntersection(float d1, float d2) { return max(d1, d2); }
float opDifference(float d1, float d2) { return max(d1, -d2); }

// ------------------------
// Noise/FBM utilities for terrain
// ------------------------
float hash12(vec2 p) {
    // Dave Hoskins–style hash
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p, float seed) {
    // Seed offset keeps determinism per-object
    vec2 so = vec2(seed * 57.0, seed * 113.0);
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i + so);
    float b = hash12(i + vec2(1.0, 0.0) + so);
    float c = hash12(i + vec2(0.0, 1.0) + so);
    float d = hash12(i + vec2(1.0, 1.0) + so);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm2D(vec2 p, float baseFreq, int octaves, float lacunarity, float gain, float seed, float warpStrength, float warpToggle, float ridgedToggle) {
    // Optional light domain warp for richer features
    vec2 pp = p;
    if (warpToggle > 0.5 && warpStrength > 0.0) {
        float wf = max(0.01, baseFreq * 0.5);
        float wx = valueNoise(p * wf + vec2(13.1 * seed, 37.7 * seed), seed);
        float wy = valueNoise(p * wf + vec2(91.4 * seed + 17.0, 27.9 * seed + 11.0), seed);
        pp += (vec2(wx, wy) * 2.0 - 1.0) * warpStrength;
    }

    float amp = 1.0;
    float freq = baseFreq;
    float sum = 0.0;
    int oct = 8;
    for (int i = 0; i < oct; ++i) {
        float n = valueNoise(pp * freq + vec2(17.0 * seed, 29.0 * seed), seed);
        if (ridgedToggle > 0.5) {
            n = 1.0 - abs(2.0 * n - 1.0);
        }
        sum += n * amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

float terrainHeightAt(vec3 p, vec3 originXZ_seed, float amplitude, float baseFreq, vec3 oct_lac_gain, vec3 warpRidged) {
    // origin.xz is horizontal origin; origin.y encodes seed
    vec2 xy = p.xy - vec2(originXZ_seed.x, originXZ_seed.z);
    float seed = originXZ_seed.y;
    float octF = floor(oct_lac_gain.x + 0.5); // round to nearest
    float lac = oct_lac_gain.y;
    float g = oct_lac_gain.z;
    int oct = int(clamp(octF, 1.0, 8.0));
    float height = amplitude * fbm2D(xy, baseFreq, oct, lac, g, seed, warpRidged.x, warpRidged.z, warpRidged.y);
    return height;
}

float terrainSDF(vec3 p, int idx) {
    // Distance estimator compatible with sphere tracing with Z-up: d = z - height(xy)
    float h = terrainHeightAt(p, u_objPos[idx], u_objRadius[idx], u_objRadius2[idx], u_objNormal[idx], u_objColor2[idx]);
    return p.z - h - u_objExtra[idx];
}

float slopeFactorFromNormal(vec3 n) {
    // Convenience hook for materials: higher on steep slopes (0 on flat)
    // Z is up in this project
    return 1.0 - clamp(n.z, 0.0, 1.0);
}

// ------------------------
// Scene distance
// ------------------------
float sceneDistance(vec3 p, out int hitIndex) {
    float minD = 1e20;
    hitIndex = -1;

    for (int i = 0; i < u_objCount; ++i) {
        float t = u_objType[i];
        float d = 1e20;

        if (t < 0.5) {
            d = sphereSDF(p, u_objPos[i], u_objRadius[i]);
        } else if (t < 1.5) {
            d = planeSDF(p, u_objPos[i], u_objNormal[i]);
        } else if (t < 2.5) {
            d = boxSDF(p, u_objPos[i], vec3(u_objRadius[i]));
        } else if (t < 3.5) {
            d = cylinderSDF(p, u_objPos[i], u_objRadius[i], u_objRadius[i]*2.0);
        } else if (t < 4.5) {
            d = capsuleSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i]);
        } else if (t < 5.5) {
            d = torusSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i]);
        } else if (t < 6.5) {
            // Union of two spheres
            float d1 = sphereSDF(p, u_objPos[i], u_objRadius[i]);
            float d2 = sphereSDF(p, u_objNormal[i], u_objRadius2[i]);
            d = min(d1, d2);
        } else if (t < 7.5) {
            // Intersection of two spheres
            float d1 = sphereSDF(p, u_objPos[i], u_objRadius[i]);
            float d2 = sphereSDF(p, u_objNormal[i], u_objRadius2[i]);
            d = max(d1, d2);
        } else if (t < 8.5) {
            // Difference of two spheres
            float d1 = sphereSDF(p, u_objPos[i], u_objRadius[i]);
            float d2 = sphereSDF(p, u_objNormal[i], u_objRadius2[i]);
            d = max(d1, -d2);
        } else if (t < 9.5) {
            // Mandelbulb fractal
            d = mandelbulbSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i], u_objNormal[i].x);
        } else if (t < 10.5) {
            // Procedural terrain heightfield
            d = terrainSDF(p, i);
        } else if (t < 11.5) {
            // Quaternion Julia set
            vec3 juliaC = vec3(u_objNormal[i].y, u_objNormal[i].z, u_objRadius2[i]);
            d = quaternionJuliaSDF(p, u_objPos[i], u_objRadius[i], juliaC, u_objNormal[i].x);
        }

        if (d < minD) { minD = d; hitIndex = i; }
    }

    return minD;
}

// ------------------------
// Normal estimation
// ------------------------
vec3 estimateNormal(vec3 p) {
    const float e = 0.001;
    int dummy;
    float dx = sceneDistance(p + vec3(e,0,0), dummy) - sceneDistance(p - vec3(e,0,0), dummy);
    float dy = sceneDistance(p + vec3(0,e,0), dummy) - sceneDistance(p - vec3(0,e,0), dummy);
    float dz = sceneDistance(p + vec3(0,0,e), dummy) - sceneDistance(p - vec3(0,0,e), dummy);
    return normalize(vec3(dx,dy,dz));
}

// ------------------------
// UV coordinate calculation
// ------------------------
vec2 calculateSphereUV(vec3 localPos, vec3 normal) {
    // Convert to spherical coordinates
    // localPos is the position relative to sphere center
    vec3 dir = normalize(localPos);

    // Calculate spherical coordinates
    // theta: polar angle (0 to PI) - from +Z axis
    // phi: azimuthal angle (0 to 2*PI) - around Z axis
    float theta = acos(clamp(dir.z, -1.0, 1.0)); // 0 to PI
    float phi = atan(dir.y, dir.x); // -PI to PI

    // Map to UV coordinates [0, 1]
    float u = (phi + 3.14159265359) / (2.0 * 3.14159265359); // 0 to 1
    float v = theta / 3.14159265359; // 0 to 1

    return vec2(u, v);
}

vec2 calculateBoxUV(vec3 localPos, vec3 normal, float boxSize) {
    // Determine which face was hit by checking the normal
    vec3 absNormal = abs(normal);
    vec2 texUV;

    if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
        // Hit X face (left or right)
        texUV = vec2(localPos.y, localPos.z) / (2.0 * boxSize) + 0.5;
        if (normal.x < 0.0) texUV.x = 1.0 - texUV.x; // Flip for left face
    } else if (absNormal.y > absNormal.z) {
        // Hit Y face (front or back)
        texUV = vec2(localPos.x, localPos.z) / (2.0 * boxSize) + 0.5;
        if (normal.y < 0.0) texUV.x = 1.0 - texUV.x; // Flip for back face
    } else {
        // Hit Z face (top or bottom)
        texUV = vec2(localPos.x, localPos.y) / (2.0 * boxSize) + 0.5;
        if (normal.z < 0.0) texUV.y = 1.0 - texUV.y; // Flip for bottom face
    }

    return texUV;
}

// ------------------------
// Shadow ray marching
// ------------------------
float shadowRay(vec3 p, vec3 normal, vec3 lightDir) {
    // Offset from surface to avoid self-intersection
    // Use a larger bias and also offset along light direction
    const float shadowBias = 0.02;
    vec3 shadowOrigin = p + normal * shadowBias + lightDir * shadowBias;

    // Ray march toward light
    const float SHADOW_EPS = 0.005;  // Slightly larger epsilon to avoid surface acne
    const float MAX_SHADOW_DIST = 100.0;
    const int MAX_SHADOW_STEPS = 64;

    float distTraveled = 0.0;

    // Start with a minimum step to ensure we're away from the surface
    distTraveled = shadowBias * 2.0;

    for (int i = 0; i < MAX_SHADOW_STEPS && distTraveled < MAX_SHADOW_DIST; ++i) {
        int dummy;
        vec3 currentPos = shadowOrigin + lightDir * distTraveled;
        float d = sceneDistance(currentPos, dummy);

        // If we hit something, we're in shadow
        if (d < SHADOW_EPS) {
            return 0.0;  // Hard shadow: completely dark
        }

        // Step forward by the distance (with minimum step to avoid getting stuck)
        distTraveled += max(d, 0.02);
    }

    // No occlusion found, fully lit
    return 1.0;
}

// ------------------------
// Helpers (non-recursive reflections)
// ------------------------

vec3 baseColorAt(int hitIndex, vec3 p) {
    // For CSG types compute child sphere distances so we can pick a color
    if (u_objType[hitIndex] >= 6.0 && u_objType[hitIndex] <= 8.0) {
        float d1 = sphereSDF(p, u_objPos[hitIndex], u_objRadius[hitIndex]);
        float d2 = sphereSDF(p, u_objNormal[hitIndex], u_objRadius2[hitIndex]);
        if (u_objType[hitIndex] == 6.0) { // Union
            return (d1 < d2) ? u_objColor[hitIndex] : u_objColor2[hitIndex];
        } else if (u_objType[hitIndex] == 7.0) { // Intersection
            return (d1 < d2) ? u_objColor[hitIndex] : u_objColor2[hitIndex];
        } else { // Difference
            return u_objColor[hitIndex];
        }
    }
    return u_objColor[hitIndex];
}


vec3 shadePhong(vec3 p, vec3 normal, vec3 viewDir, int hitIndex) {
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);

    vec3 specReflectDir = reflect(-lightDir, normal);
    float specular = pow(max(dot(viewDir, specReflectDir), 0.0), 32.0);  // 32 = shininess

    float shadow = shadowRay(p, normal, lightDir);

    vec3 base = baseColorAt(hitIndex, p);

    vec3 ambient = base * 0.2;
    vec3 diffuse = base * 0.6 * lambert * shadow;
    vec3 specularColor = vec3(1.0) * 0.2 * specular * shadow;
    return ambient + diffuse + specularColor;
}

bool rayMarch(vec3 ro, vec3 rd, out vec3 hitPos, out int hitIndex) {
    const float EPS = 0.001;
    const float MAX_DIST = 2000.0;
    const int MAX_STEPS = 512;

    vec3 p = ro;
    float distTraveled = 0.0;
    hitIndex = -1;

    for (int i = 0; i < MAX_STEPS && distTraveled < MAX_DIST; ++i) {
        int tmp;
        float d = sceneDistance(p, tmp);
        if (d < EPS) { hitIndex = tmp; break; }
        p += max(d, 0.0) * rd;
        distTraveled += max(d, 0.0);
    }

    hitPos = p;
    return hitIndex != -1;
}

// Trace the *reflected* path (non-recursive). Returns the reflected radiance seen along a ray.
vec3 traceReflectionPath(vec3 rayOrigin, vec3 rayDir) {
    vec3 accum = vec3(0.0);
    float throughput = 1.0;

    for (int bounce = 0; bounce < MAX_REFLECTION_DEPTH; ++bounce) {
        vec3 hitPos;
        int hitIndex;
        bool hit = rayMarch(rayOrigin, rayDir, hitPos, hitIndex);

        if (!hit) {
            accum += throughput * skyColor();
            break;
        }

        vec3 n = estimateNormal(hitPos);
        vec3 viewDir = normalize(-rayDir);

        // Local shading of what we see in the reflection
        vec3 local = shadePhong(hitPos, n, viewDir, hitIndex);
        accum += throughput * local;

        // Continue reflecting if the hit surface is reflective
        float refl = 0.0;
        if (hitIndex >= 0 && hitIndex < u_objCount) {
            refl = clamp(u_objReflectivity[hitIndex], 0.0, 1.0);
        }
        throughput *= refl;

        if (throughput < 0.01) {
            break;
        }

        rayOrigin = hitPos + n * REFLECTION_BIAS;
        rayDir = normalize(reflect(rayDir, n));
    }

    return accum;
}

// ------------------------
// Main
// ------------------------
void main() {
    vec2 uv = (gl_FragCoord.xy / u_resolution) * 2.0 - 1.0;
    uv.x *= u_resolution.x / u_resolution.y;

    float f = tan(u_fov * 0.5);
    vec3 dir = normalize(u_camForward + uv.x * f * u_camRight + uv.y * f * u_camUp);
    vec3 ro = u_camOrigin;
    vec3 p = ro;
    vec3 rayDir = normalize(u_camForward + uv.x * f * u_camRight + uv.y * f * u_camUp);
    vec3 rayOrigin = u_camOrigin;

    // Primary hit
    vec3 hitPos;
    int hitIndex;
    if (!rayMarch(rayOrigin, rayDir, hitPos, hitIndex)) {
        gl_FragColor = vec4(skyColor(), 1.0);
        return;
    }
    vec3 n0 = estimateNormal(hitPos);
    vec3 viewDir0 = normalize(-rayDir);
    vec3 local0 = shadePhong(hitPos, n0, viewDir0, hitIndex);


    vec3 normal = estimateNormal(p);
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);

    float refl0 = 0.0;
    if (hitIndex >= 0 && hitIndex < u_objCount) {
        refl0 = clamp(u_objReflectivity[hitIndex], 0.0, 1.0);
    }

    // Keep the full local shading, then add reflected contribution on top.
    // This is intentionally "brighter" than energy conservation to avoid dull results.
    vec3 color = local0;
    if (MAX_REFLECTION_DEPTH > 0 && refl0 > 0.001) {
        vec3 reflDir0 = normalize(reflect(rayDir, n0));
        vec3 reflOrigin0 = hitPos + n0 * REFLECTION_BIAS;
        vec3 reflected = traceReflectionPath(reflOrigin0, reflDir0);
        color += refl0 * REFLECTION_STRENGTH * reflected;
    // Calculate UV coordinates and sample texture for supported object types
    vec3 localPos = p - u_objPos[hitIndex];
    vec2 texUV;
    bool useTexture = false;
    float textureIndex = u_objTextureIndex[hitIndex];

    // Only use texture if object has a valid texture index (>= 0)
    if (textureIndex >= 0.0) {
        // Determine UV coordinates based on object type
        if (u_objType[hitIndex] < 0.5) {
            // Sphere - use spherical coordinates
            texUV = calculateSphereUV(localPos, normal);
            useTexture = true;
        } else if (u_objType[hitIndex] >= 2.0 && u_objType[hitIndex] < 2.5) {
            // Box - use face-based mapping
            texUV = calculateBoxUV(localPos, normal, u_objRadius[hitIndex]);
            useTexture = true;
        } else if (u_objType[hitIndex] >= 9.0 && u_objType[hitIndex] < 9.5) {
            // Mandelbulb - use spherical coordinates (similar to sphere)
            texUV = calculateSphereUV(localPos, normal);
            useTexture = true;
        } else if (u_objType[hitIndex] >= 10.0 && u_objType[hitIndex] < 10.5) {
            // Quaternion Julia - use spherical coordinates (similar to sphere)
            texUV = calculateSphereUV(localPos, normal);
            useTexture = true;
        }
    }

    // For CSG types compute child sphere distances so we can pick a color
    vec3 base;
    if (u_objType[hitIndex] >= 6.0 && u_objType[hitIndex] <= 8.0) {
        float d1 = sphereSDF(p, u_objPos[hitIndex], u_objRadius[hitIndex]);
        float d2 = sphereSDF(p, u_objNormal[hitIndex], u_objRadius2[hitIndex]);
        if (u_objType[hitIndex] == 6.0) { // Union
            base = (d1 < d2) ? u_objColor[hitIndex] : u_objColor2[hitIndex];
        } else if (u_objType[hitIndex] == 7.0) { // Intersection
            // For intersection the visible surface may belong to either sphere; choose closer
            base = (d1 < d2) ? u_objColor[hitIndex] : u_objColor2[hitIndex];
        } else { // Difference
            // Difference shows the first object's surface (a \ b)
            base = u_objColor[hitIndex];
        }
    } else if (useTexture) {
        // Sample texture for spheres, boxes, and mandelbulbs
        // Use texture index to select the correct texture
        int texIdx = int(textureIndex);
        vec4 texColor;

        // Select texture based on index (GLSL doesn't support dynamic sampler array indexing)
        if (texIdx == 0) {
            texColor = texture2D(u_texture0, texUV);
        } else if (texIdx == 1) {
            texColor = texture2D(u_texture1, texUV);
        } else if (texIdx == 2) {
            texColor = texture2D(u_texture2, texUV);
        } else if (texIdx == 3) {
            texColor = texture2D(u_texture3, texUV);
        } else if (texIdx == 4) {
            texColor = texture2D(u_texture4, texUV);
        } else if (texIdx == 5) {
            texColor = texture2D(u_texture5, texUV);
        } else if (texIdx == 6) {
            texColor = texture2D(u_texture6, texUV);
        } else if (texIdx == 7) {
            texColor = texture2D(u_texture7, texUV);
        } else {
            // Fallback to solid color if texture index is out of range
            texColor = vec4(u_objColor[hitIndex], 1.0);
        }
        base = texColor.rgb;
    } else {
        // Use solid color for other object types
        base = u_objColor[hitIndex];
    }

    // Prevent extreme blowouts
    color = clamp(color, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
}