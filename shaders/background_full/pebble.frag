#version 330 core

in vec3 worldPos;
in vec3 worldNormal;

uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform float time;
uniform int causticsMode;
uniform float waterBaseY;
uniform float causticStrength;
uniform float causticFade;
uniform float waveStrength;

out vec4 fragColor;

vec2 hash22(vec2 p) {
    vec2 q = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(q) * 43758.5453);
}

float worleyBorder(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    float nearestDistance = 10.0;
    float secondDistance = 10.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y));
            vec2 point = 0.5 + 0.32 * sin(time * 0.045 + 6.2831 * hash22(cell + offset));
            float distanceSquared = dot(offset + point - local, offset + point - local);
            if (distanceSquared < nearestDistance) {
                secondDistance = nearestDistance;
                nearestDistance = distanceSquared;
            } else if (distanceSquared < secondDistance) {
                secondDistance = distanceSquared;
            }
        }
    }
    return sqrt(secondDistance) - sqrt(nearestDistance);
}

vec3 getWaterNormal(vec2 xz) {
    const vec2 directions[6] = vec2[6](
        vec2(0.957826, 0.287348), vec2(-0.514496, 0.857493),
        vec2(0.894427, -0.447214), vec2(-0.980581, -0.196116),
        vec2(0.316228, 0.948683), vec2(-0.707107, 0.707107)
    );
    const float amplitudes[6] = float[6](0.20, 0.12, 0.08, 0.05, 0.035, 0.025);
    const float frequencies[6] = float[6](0.60, 1.20, 2.00, 3.50, 4.60, 5.80);
    const float speeds[6] = float[6](0.80, 1.10, 1.60, 2.20, 1.85, 2.55);
    const float phaseOffsets[6] = float[6](0.0, 1.7, 3.1, 4.6, 2.3, 5.4);
    vec2 derivative = vec2(0.0);
    for (int i = 0; i < 6; ++i) {
        float phase = dot(directions[i], xz) * frequencies[i] + speeds[i] * time + phaseOffsets[i];
        derivative += amplitudes[i] * frequencies[i] * directions[i] * cos(phase);
    }
    return normalize(vec3(-derivative.x * waveStrength, 1.0, -derivative.y * waveStrength));
}

vec2 refractedFloorHit(vec2 surfaceXZ, float targetY) {
    vec3 ray = refract(normalize(sunDirection), getWaterNormal(surfaceXZ), 1.0 / 1.333);
    if (dot(ray, ray) < 0.0001 || ray.y >= -0.001) return vec2(10000.0);
    return surfaceXZ + ray.xz * max(waterBaseY - targetY, 0.0) / -ray.y;
}

float snellConcentration(vec3 position, float web) {
    float epsilon = 0.18;
    vec2 center = refractedFloorHit(position.xz, position.y);
    vec2 hitX = refractedFloorHit(position.xz + vec2(epsilon, 0.0), position.y);
    vec2 hitZ = refractedFloorHit(position.xz + vec2(0.0, epsilon), position.y);
    vec2 dx = (hitX - center) / epsilon;
    vec2 dz = (hitZ - center) / epsilon;
    float area = abs(dx.x * dz.y - dx.y * dz.x);
    float concentration = clamp(1.0 / max(area, 0.20), 0.0, 2.4);
    return clamp(web * mix(0.25, 1.35, smoothstep(0.65, 2.20, concentration)), 0.0, 1.20);
}

void main() {
    vec3 normal = normalize(worldNormal);
    float diffuse = max(dot(normal, -sunDirection), 0.0);
    float variation = 0.5 + 0.5 * sin(worldPos.x * 3.7 + worldPos.z * 4.1);
    vec3 baseColor = mix(vec3(0.20, 0.19, 0.16), vec3(0.42, 0.39, 0.30), variation);
    vec3 color = baseColor * (vec3(0.46, 0.54, 0.52) + vec3(0.60, 0.63, 0.49) * diffuse);
    float distanceToCamera = length(worldPos - cameraPos);
    vec2 causticUv = worldPos.xz * 2.90 + getWaterNormal(worldPos.xz).xz * 0.30;
    causticUv += vec2(time * 0.0045, time * 0.0028);
    float caustic = 1.0 - smoothstep(0.018, 0.075, worleyBorder(causticUv));
    caustic = clamp(smoothstep(0.30, 0.86, caustic), 0.0, 0.88);
    if (causticsMode == 1) caustic = snellConcentration(worldPos, caustic);
    float upwardMask = max(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0);
    float lightFacing = max(dot(normal, -sunDirection), 0.0);
    float surfaceMask = mix(0.12, 1.0, pow(upwardMask, 0.72)) * mix(0.65, 1.0, lightFacing);
    color += vec3(1.0, 0.98, 0.82) * caustic * surfaceMask
        * causticStrength * exp(-distanceToCamera * causticFade);
    float fogFactor = clamp(1.0 - exp(-distanceToCamera * 0.06), 0.0, 1.0);
    color = mix(color, vec3(0.0, 0.52, 0.62), fogFactor);
    fragColor = vec4(color, 1.0);
}
