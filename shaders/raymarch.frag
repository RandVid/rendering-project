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
uniform float u_objReflectivity[MAX_OBJECTS];

// Reflection depth (0 = no reflections)
const int MAX_REFLECTION_DEPTH = 2;

// Reflection tuning
const float REFLECTION_BIAS = 0.02;
// Artistic boost for reflected contribution (not physically correct, but avoids "too dark" look)
const float REFLECTION_STRENGTH = 0.9;

// ------------------------
// Indirect diffuse (very small GI approximation)
// ------------------------
// 0 = off, 1 = on
const int INDIRECT_ENABLED = 1;
// How many hemisphere samples per bounce (1 = cheapest)
const int INDIRECT_SAMPLES = 5;
// How many diffuse bounces to trace (1 = classic "one-bounce GI")
const int INDIRECT_DEPTH = 1;
// Strength of the indirect contribution
const float INDIRECT_STRENGTH = 0.1;
// Bias to avoid self-intersection when shooting indirect rays
const float INDIRECT_BIAS = 0.03;
// Clamp indirect samples to reduce "fireflies" (bright noisy pixels)
const float INDIRECT_SAMPLE_CLAMP = 1.0;

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

// Example: CSG operations
float opUnion(float d1, float d2) { return min(d1, d2); }
float opIntersection(float d1, float d2) { return max(d1, d2); }
float opDifference(float d1, float d2) { return max(d1, -d2); }

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

// Indirect GI should be *diffuse only* to keep variance low (specular causes fireflies).
vec3 shadeDiffuseOnly(vec3 p, vec3 normal, int hitIndex) {
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);
    float shadow = shadowRay(p, normal, lightDir);

    vec3 base = baseColorAt(hitIndex, p);

    // Same coefficients as your main shading, but no specular.
    vec3 ambient = base * 0.2;
    vec3 diffuse = base * 0.6 * lambert * shadow;
    return ambient + diffuse;
}

bool rayMarch(vec3 ro, vec3 rd, out vec3 hitPos, out int hitIndex) {
    const float EPS = 0.001;
    const float MAX_DIST = 200.0;
    const int MAX_STEPS = 128;

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

// ------------------------
// Random + cosine-weighted hemisphere sampling (no textures)
// ------------------------
float hash12(vec2 p) {
    // Cheap hash: stable per pixel and varies with p
    vec3 p3  = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 cosineSampleHemisphere(float u1, float u2) {
    // Cosine-weighted hemisphere in tangent space (z = up)
    float r = sqrt(u1);
    float phi = 6.28318530718 * u2;
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(max(0.0, 1.0 - u1));
    return vec3(x, y, z);
}

mat3 makeTBN(vec3 n) {
    // Build an orthonormal basis around n
    vec3 up = (abs(n.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

// Trace a tiny indirect diffuse path (non-recursive). This is a Monte Carlo estimate:
// - sample hemisphere directions
// - march to find what they hit
// - use that hit's local shading as incoming radiance
vec3 traceIndirectDiffuse(vec3 p0, vec3 n0, vec3 primaryAlbedo) {
    if (INDIRECT_ENABLED == 0 || INDIRECT_DEPTH <= 0 || INDIRECT_SAMPLES <= 0) {
        return vec3(0.0);
    }

    // Start just above the surface
    vec3 rayOrigin = p0 + n0 * INDIRECT_BIAS;
    vec3 curNormal = n0;

    vec3 accum = vec3(0.0);
    vec3 throughput = primaryAlbedo * INDIRECT_STRENGTH;

    // Deterministic seed per pixel
    vec2 baseSeed = gl_FragCoord.xy;

    // Path trace a few diffuse bounces
    for (int bounce = 0; bounce < INDIRECT_DEPTH; ++bounce) {
        vec3 bounceSum = vec3(0.0);

        for (int s = 0; s < INDIRECT_SAMPLES; ++s) {
            // Vary seed by bounce/sample to decorrelate
            float u1 = hash12(baseSeed + vec2(float(bounce * 37 + s * 11), 1.23));
            float u2 = hash12(baseSeed + vec2(float(bounce * 19 + s * 23), 4.56));

            vec3 localDir = cosineSampleHemisphere(u1, u2);
            vec3 worldDir = normalize(makeTBN(curNormal) * localDir);

            vec3 hitPos;
            int hitIndex;
            bool hit = rayMarch(rayOrigin, worldDir, hitPos, hitIndex);

            if (!hit) {
                bounceSum += skyColor();
                continue;
            }

            vec3 hn = estimateNormal(hitPos);
            // Diffuse-only indirect (much lower variance than Phong)
            vec3 Li = shadeDiffuseOnly(hitPos, hn, hitIndex);
            // Firefly clamp (prevents rare bright samples from dominating)
            Li = min(Li, vec3(INDIRECT_SAMPLE_CLAMP));
            bounceSum += Li;
        }

        vec3 Li = bounceSum / float(INDIRECT_SAMPLES);
        accum += throughput * Li;

        // For >1 bounce, walk the path using one representative direction (the same MC sample)
        // This keeps it cheap and stable, but it's still just an approximation.
        if (bounce + 1 >= INDIRECT_DEPTH) {
            break;
        }

        // Pick one direction for path continuation (reuse sample 0)
        float u1c = hash12(baseSeed + vec2(float(bounce * 37), 7.89));
        float u2c = hash12(baseSeed + vec2(float(bounce * 19), 9.87));
        vec3 localDirC = cosineSampleHemisphere(u1c, u2c);
        vec3 worldDirC = normalize(makeTBN(curNormal) * localDirC);

        vec3 hitPosC;
        int hitIndexC;
        bool hitC = rayMarch(rayOrigin, worldDirC, hitPosC, hitIndexC);
        if (!hitC) {
            break;
        }

        vec3 hnC = estimateNormal(hitPosC);
        vec3 albedoC = baseColorAt(hitIndexC, hitPosC);

        throughput *= albedoC;
        if (max(throughput.r, max(throughput.g, throughput.b)) < 0.01) {
            break;
        }

        rayOrigin = hitPosC + hnC * INDIRECT_BIAS;
        curNormal = hnC;
    }

    return accum;
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

    float refl0 = 0.0;
    if (hitIndex >= 0 && hitIndex < u_objCount) {
        refl0 = clamp(u_objReflectivity[hitIndex], 0.0, 1.0);
    }

    // Keep the full local shading, then add reflected contribution on top.
    // This is intentionally "brighter" than energy conservation to avoid dull results.
    vec3 color = local0;

    // Add 1-sample indirect diffuse bounce (GI approximation)
    vec3 primaryAlbedo = baseColorAt(hitIndex, hitPos);
    color += traceIndirectDiffuse(hitPos, n0, primaryAlbedo);

    if (MAX_REFLECTION_DEPTH > 0 && refl0 > 0.001) {
        vec3 reflDir0 = normalize(reflect(rayDir, n0));
        vec3 reflOrigin0 = hitPos + n0 * REFLECTION_BIAS;
        vec3 reflected = traceReflectionPath(reflOrigin0, reflDir0);
        color += refl0 * REFLECTION_STRENGTH * reflected;
    }

    // Prevent extreme blowouts
    color = clamp(color, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
}