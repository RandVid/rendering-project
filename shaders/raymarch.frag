uniform vec2 u_resolution;
uniform vec3 u_camOrigin;
uniform vec3 u_camForward;
uniform vec3 u_camRight;
uniform vec3 u_camUp;
uniform float u_fov;
uniform vec3 u_light;
uniform int u_objCount;
uniform sampler2D u_boxTexture;

const int MAX_OBJECTS = 32;
uniform vec3 u_objPos[MAX_OBJECTS];
uniform vec3 u_objColor[MAX_OBJECTS];
uniform vec3 u_objColor2[MAX_OBJECTS];
uniform vec3 u_objNormal[MAX_OBJECTS];
uniform float u_objRadius[MAX_OBJECTS];
uniform float u_objRadius2[MAX_OBJECTS];
uniform float u_objType[MAX_OBJECTS];
uniform float u_objTextureIndex[MAX_OBJECTS];

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
// Main
// ------------------------
void main() {
    vec2 uv = (gl_FragCoord.xy / u_resolution) * 2.0 - 1.0;
    uv.x *= u_resolution.x / u_resolution.y;

    float f = tan(u_fov * 0.5);
    vec3 dir = normalize(u_camForward + uv.x * f * u_camRight + uv.y * f * u_camUp);
    vec3 ro = u_camOrigin;
    vec3 p = ro;

    const float EPS = 0.001;
    const float MAX_DIST = 200.0;
    const int MAX_STEPS = 128;

    float distTraveled = 0.0;
    int hitIndex = -1;
    for (int i = 0; i < MAX_STEPS && distTraveled < MAX_DIST; ++i) {
        int tmp;
        float d = sceneDistance(p, tmp);
        if (d < EPS) { hitIndex = tmp; break; }
        p += max(d, 0.0) * dir;
        distTraveled += max(d, 0.0);
    }

    if (hitIndex == -1) {
        // Blue sky background
        gl_FragColor = vec4(0.5, 0.7, 1.0, 1.0);
        return;
    }

    vec3 normal = estimateNormal(p);
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);

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
        vec4 texColor = texture2D(u_boxTexture, texUV);
        base = texColor.rgb;
    } else {
        // Use solid color for other object types
        base = u_objColor[hitIndex];
    }

    vec3 color = base * (0.2 + 0.8 * lambert);
    gl_FragColor = vec4(color, 1.0);
}