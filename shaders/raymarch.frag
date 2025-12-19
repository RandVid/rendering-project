uniform vec2 u_resolution;
uniform vec3 u_camOrigin;
uniform vec3 u_camForward;
uniform vec3 u_camRight;
uniform vec3 u_camUp;
uniform float u_fov;
uniform vec3 u_light;
uniform int u_objCount;

const int MAX_OBJECTS = 32;
uniform vec3  u_objPos[MAX_OBJECTS];
uniform vec3  u_objColor[MAX_OBJECTS];
uniform vec3  u_objColor2[MAX_OBJECTS];
uniform vec3  u_objNormal[MAX_OBJECTS];
uniform float u_objRadius[MAX_OBJECTS];
uniform float u_objRadius2[MAX_OBJECTS];
uniform float u_objType[MAX_OBJECTS];

// from reflections shader (terrain offset / extra param)
uniform float u_objExtra[MAX_OBJECTS];
uniform float u_objReflectivity[MAX_OBJECTS];

// from textures shader
uniform float u_objTextureIndex[MAX_OBJECTS];
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform sampler2D u_texture5;
uniform sampler2D u_texture6;
uniform sampler2D u_texture7;

// Reflection depth (0 = no reflections)
const int MAX_REFLECTION_DEPTH = 2;

// Reflection tuning
const float REFLECTION_BIAS = 0.02;
const float REFLECTION_STRENGTH = 0.9;

vec3 skyColor() { return vec3(0.5, 0.7, 1.0); }

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
    float distFromCenter = length(p - center);
    float boundingRadius = scale * 3.0;
    if (distFromCenter > boundingRadius * 3.0) {
        return distFromCenter - boundingRadius;
    }

    vec3 c = (p - center) / scale;
    vec3 z = vec3(0.0);
    float dr = 1.0;
    float bailout = 2.0;

    for (float i = 0.0; i < iterations; i += 1.0) {
        float r = length(z);
        if (r > bailout) break;

        if (r < 1e-10) {
            r = 1e-10;
            z = vec3(1e-10, 0.0, 0.0);
        }

        float zr_ratio = clamp(z.z / r, -1.0, 1.0);
        float theta = acos(zr_ratio);
        float phi = atan(z.y, z.x);

        dr = pow(r, power - 1.0) * power * dr + 1.0;

        float zr = pow(r, power);
        theta *= power;
        phi *= power;

        z = vec3(
        sin(theta) * cos(phi),
        sin(theta) * sin(phi),
        cos(theta)
        ) * zr;

        z += c;
    }

    float r = length(z);
    if (r < 1e-10) r = 1e-10;
    if (dr < 1e-10) dr = 1e-10;

    float distance = 0.5 * log(r) * r / dr;
    distance *= scale;

    if (distance < 0.0) distance = 0.0005;
    if (distance < 0.0001) distance = 0.0001;
    if (distance != distance || distance > 100.0) distance = 100.0;

    return distance;
}

float quaternionJuliaSDF(vec3 p, vec3 center, float scale, vec3 juliaC, float iterations) {
    float distFromCenter = length(p - center);
    float boundingRadius = scale * 2.0;
    if (distFromCenter > boundingRadius * 3.0) {
        return distFromCenter - boundingRadius;
    }

    vec3 z = (p - center) / scale;
    float dr = 1.0;
    float bailout = 2.0;

    for (float i = 0.0; i < iterations; i += 1.0) {
        float r = length(z);
        if (r > bailout) break;

        if (r < 1e-10) {
            r = 1e-10;
            z = vec3(1e-10, 0.0, 0.0);
        }

        float x = z.x;
        float y = z.y;
        float zz = z.z;

        vec3 zSquared = vec3(
        x * x - y * y - zz * zz,
        2.0 * x * y,
        2.0 * x * zz
        );

        z = zSquared + juliaC;
        dr = 2.0 * r * dr + 1.0;
    }

    float r = length(z);
    if (r < 1e-10) r = 1e-10;
    if (dr < 1e-10) dr = 1e-10;

    float distance = 0.5 * log(r) * r / dr;
    distance *= scale;

    if (distance < 0.0) distance = 0.0005;
    if (distance < 0.0001) distance = 0.0001;
    if (distance != distance || distance > 100.0) distance = 100.0;

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
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p, float seed) {
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
    vec2 xy = p.xy - vec2(originXZ_seed.x, originXZ_seed.z);
    float seed = originXZ_seed.y;
    float octF = floor(oct_lac_gain.x + 0.5);
    float lac = oct_lac_gain.y;
    float g = oct_lac_gain.z;
    int oct = int(clamp(octF, 1.0, 8.0));
    float height = amplitude * fbm2D(xy, baseFreq, oct, lac, g, seed, warpRidged.x, warpRidged.z, warpRidged.y);
    return height;
}

float terrainSDF(vec3 p, int idx) {
    float h = terrainHeightAt(p, u_objPos[idx], u_objRadius[idx], u_objRadius2[idx], u_objNormal[idx], u_objColor2[idx]);
    return p.z - h - u_objExtra[idx];
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
            // Box - use full size vector from objNormal
            d = boxSDF(p, u_objPos[i], u_objNormal[i]);
        } else if (t < 3.5) {
            d = cylinderSDF(p, u_objPos[i], u_objRadius[i], u_objRadius[i]*2.0);
        } else if (t < 4.5) {
            d = capsuleSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i]);
        } else if (t < 5.5) {
            d = torusSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i]);
        } else if (t < 6.5) {
            float d1 = sphereSDF(p, u_objPos[i], u_objRadius[i]);
            float d2 = sphereSDF(p, u_objNormal[i], u_objRadius2[i]);
            d = min(d1, d2);
        } else if (t < 7.5) {
            float d1 = sphereSDF(p, u_objPos[i], u_objRadius[i]);
            float d2 = sphereSDF(p, u_objNormal[i], u_objRadius2[i]);
            d = max(d1, d2);
        } else if (t < 8.5) {
            float d1 = sphereSDF(p, u_objPos[i], u_objRadius[i]);
            float d2 = sphereSDF(p, u_objNormal[i], u_objRadius2[i]);
            d = max(d1, -d2);
        } else if (t < 9.5) {
            d = mandelbulbSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i], u_objNormal[i].x);
        } else if (t < 10.5) {
            d = terrainSDF(p, i);
        } else if (t < 11.5) {
            d = quaternionJuliaSDF(p, u_objPos[i], u_objRadius[i], vec3(u_objNormal[i].y, u_objNormal[i].z, u_objRadius2[i]), u_objNormal[i].x);
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
// UV coordinate calculation (from texture shader)
// ------------------------
vec2 calculateSphereUV(vec3 localPos, vec3 normal) {
    vec3 dir = normalize(localPos);
    float theta = acos(clamp(dir.z, -1.0, 1.0));
    float phi = atan(dir.y, dir.x);
    float u = (phi + 3.14159265359) / (2.0 * 3.14159265359);
    float v = theta / 3.14159265359;
    return vec2(u, v);
}

vec2 calculateBoxUV(vec3 localPos, vec3 normal, vec3 boxSize) {
    vec3 absNormal = abs(normal);
    vec2 texUV;

    if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
        // Hit X face (left or right) - use Y and Z dimensions
        texUV = vec2(localPos.y, localPos.z) / (2.0 * vec2(boxSize.y, boxSize.z)) + 0.5;
        if (normal.x < 0.0) texUV.x = 1.0 - texUV.x; // Flip for left face
    } else if (absNormal.y > absNormal.z) {
        // Hit Y face (front or back) - use X and Z dimensions
        texUV = vec2(localPos.x, localPos.z) / (2.0 * vec2(boxSize.x, boxSize.z)) + 0.5;
        if (normal.y < 0.0) texUV.x = 1.0 - texUV.x; // Flip for back face
    } else {
        // Hit Z face (top or bottom) - use X and Y dimensions
        texUV = vec2(localPos.x, localPos.y) / (2.0 * vec2(boxSize.x, boxSize.y)) + 0.5;
        if (normal.z < 0.0) texUV.y = 1.0 - texUV.y; // Flip for bottom face
    }

    return texUV;
}

vec4 sampleTextureByIndex(int texIdx, vec2 uv) {
    if (texIdx == 0) return texture2D(u_texture0, uv);
    if (texIdx == 1) return texture2D(u_texture1, uv);
    if (texIdx == 2) return texture2D(u_texture2, uv);
    if (texIdx == 3) return texture2D(u_texture3, uv);
    if (texIdx == 4) return texture2D(u_texture4, uv);
    if (texIdx == 5) return texture2D(u_texture5, uv);
    if (texIdx == 6) return texture2D(u_texture6, uv);
    if (texIdx == 7) return texture2D(u_texture7, uv);
    return vec4(1.0);
}

// ------------------------
// Shadow ray marching
// ------------------------
float shadowRay(vec3 p, vec3 normal, vec3 lightDir) {
    const float shadowBias = 0.02;
    vec3 shadowOrigin = p + normal * shadowBias + lightDir * shadowBias;

    const float SHADOW_EPS = 0.005;
    const float MAX_SHADOW_DIST = 100.0;
    const int MAX_SHADOW_STEPS = 64;

    float distTraveled = shadowBias * 2.0;

    for (int i = 0; i < MAX_SHADOW_STEPS && distTraveled < MAX_SHADOW_DIST; ++i) {
        int dummy;
        vec3 currentPos = shadowOrigin + lightDir * distTraveled;
        float d = sceneDistance(currentPos, dummy);
        if (d < SHADOW_EPS) return 0.0;
        distTraveled += max(d, 0.02);
    }

    return 1.0;
}

// ------------------------
// Base color with textures + CSG (merged)
// ------------------------
vec3 baseColorAt(int hitIndex, vec3 p, vec3 normal) {
    float t = u_objType[hitIndex];

    // CSG: choose color based on which primitive contributes
    if (t >= 6.0 && t <= 8.0) {
        float d1 = sphereSDF(p, u_objPos[hitIndex], u_objRadius[hitIndex]);
        float d2 = sphereSDF(p, u_objNormal[hitIndex], u_objRadius2[hitIndex]);
        if (t < 6.5) return (d1 < d2) ? u_objColor[hitIndex] : u_objColor2[hitIndex]; // union
        if (t < 7.5) return (d1 < d2) ? u_objColor[hitIndex] : u_objColor2[hitIndex]; // intersection
        return u_objColor[hitIndex]; // difference
    }

    // Texture selection (same rules as your texture shader)
    float textureIndexF = u_objTextureIndex[hitIndex];
    if (textureIndexF >= 0.0) {
        vec3 localPos = p - u_objPos[hitIndex];
        vec2 texUV;
        bool useTexture = false;

        if (t < 0.5) { // sphere
            texUV = calculateSphereUV(localPos, normal);
            useTexture = true;
        } else if (t >= 2.0 && t < 2.5) {
            // Box - use face-based mapping with actual box dimensions
            vec3 boxSize = u_objNormal[hitIndex];
            texUV = calculateBoxUV(localPos, normal, boxSize);
            useTexture = true;
        } else if (t >= 9.0 && t < 9.5) { // mandelbulb
            texUV = calculateSphereUV(localPos, normal);
            useTexture = true;
        } else if (t >= 11.0 && t < 11.5) { // quaternion julia
            texUV = calculateSphereUV(localPos, normal);
            useTexture = true;
        }

        if (useTexture) {
            int texIdx = int(textureIndexF);
            vec4 tc = sampleTextureByIndex(texIdx, texUV);
            // If your textures have alpha you *might* want to ignore it; we keep rgb.
            return tc.rgb;
        }
    }

    // Fallback solid color
    return u_objColor[hitIndex];
}

// ------------------------
// Local shading (Phong) uses textured base
// ------------------------
vec3 shadePhong(vec3 p, vec3 normal, vec3 viewDir, int hitIndex) {
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);

    vec3 specReflectDir = reflect(-lightDir, normal);
    float specular = pow(max(dot(viewDir, specReflectDir), 0.0), 32.0);

    float shadow = shadowRay(p, normal, lightDir);

    vec3 base = baseColorAt(hitIndex, p, normal);

    vec3 ambient = base * 0.2;
    vec3 diffuse = base * 0.6 * lambert * shadow;
    vec3 specularColor = vec3(1.0) * 0.2 * specular * shadow;
    return ambient + diffuse + specularColor;
}

// ------------------------
// Ray march
// ------------------------
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

// Trace reflected radiance along a path
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

        vec3 local = shadePhong(hitPos, n, viewDir, hitIndex);
        accum += throughput * local;

        float refl = 0.0;
        if (hitIndex >= 0 && hitIndex < u_objCount) {
            refl = clamp(u_objReflectivity[hitIndex], 0.0, 1.0);
        }
        throughput *= refl;

        if (throughput < 0.01) break;

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

    vec3 color = local0;
    if (MAX_REFLECTION_DEPTH > 0 && refl0 > 0.001) {
        vec3 reflDir0 = normalize(reflect(rayDir, n0));
        vec3 reflOrigin0 = hitPos + n0 * REFLECTION_BIAS;
        vec3 reflected = traceReflectionPath(reflOrigin0, reflDir0);
        color += refl0 * REFLECTION_STRENGTH * reflected;
    }

    color = clamp(color, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
}
