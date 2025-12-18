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
uniform vec3 u_objSize[MAX_OBJECTS];
uniform vec3 u_objNormal[MAX_OBJECTS];
uniform float u_objRadius[MAX_OBJECTS];
uniform float u_objRadius2[MAX_OBJECTS];
uniform float u_objType[MAX_OBJECTS];

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
            d = boxSDF(p, u_objPos[i], u_objSize[i]);
        } else if (t < 3.5) {
            d = cylinderSDF(p, u_objPos[i], u_objRadius[i], u_objRadius2[i]*2.0);
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
        gl_FragColor = vec4(vec3(0.05), 1.0);
        return;
    }

    vec3 normal = estimateNormal(p);
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);

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
    } else {
        base = u_objColor[hitIndex];
    }

    vec3 color = base * (0.2 + 0.8 * lambert);
    gl_FragColor = vec4(color, 1.0);
}
