#version 330 core

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
uniform vec3 u_objNormal[MAX_OBJECTS];
uniform float u_objRadius[MAX_OBJECTS];
uniform float u_objType[MAX_OBJECTS];

out vec4 FragColor;

float sceneDistance(vec3 p, out int hitIndex) {
    float minD = 1e20;
    hitIndex = -1;
    for (int i = 0; i < u_objCount; ++i) {
        float t = u_objType[i];
        float d = 1e20;
        if (t < 0.5) {
            // sphere
            d = length(p - u_objPos[i]) - u_objRadius[i];
        } else {
            // plane
            d = dot(p - u_objPos[i], u_objNormal[i]);
        }
        if (d < minD) { minD = d; hitIndex = i; }
    }
    return minD;
}

vec3 estimateNormal(vec3 p) {
    const float e = 0.001;
    int dummy;
    float dx = sceneDistance(vec3(p.x + e, p.y, p.z), dummy) - sceneDistance(vec3(p.x - e, p.y, p.z), dummy);
    float dy = sceneDistance(vec3(p.x, p.y + e, p.z), dummy) - sceneDistance(vec3(p.x, p.y - e, p.z), dummy);
    float dz = sceneDistance(vec3(p.x, p.y, p.z + e), dummy) - sceneDistance(vec3(p.x, p.y, p.z - e), dummy);
    return normalize(vec3(dx, dy, dz));
}

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
        p += d * dir;
        distTraveled += d;
    }

    if (hitIndex == -1) {
        FragColor = vec4(vec3(0.05), 1.0);
        return;
    }

    vec3 normal = estimateNormal(p);
    vec3 lightDir = normalize(u_light);
    float lambert = max(dot(normal, lightDir), 0.0);
    vec3 base = u_objColor[hitIndex];
    vec3 color = base * (0.2 + 0.8 * lambert);
    FragColor = vec4(color, 1.0);
}
