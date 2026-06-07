#version 330 core

in vec3 worldPos;
in vec3 worldNormal;
in vec3 localPos;

uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform vec3 baseColor;
uniform float time;
uniform int shellDetail;
uniform int causticsMode;
uniform float waterBaseY;
uniform float causticStrength;
uniform float coralCausticStrength;
uniform float causticFade;
uniform float waveStrength;
uniform int coralCausticsDebug;

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

float waveHeight(vec2 xz) {
    const vec2 directions[6] = vec2[6](
        vec2(0.957826, 0.287348), vec2(-0.514496, 0.857493),
        vec2(0.894427, -0.447214), vec2(-0.980581, -0.196116),
        vec2(0.316228, 0.948683), vec2(-0.707107, 0.707107)
    );
    const float amplitudes[6] = float[6](0.20, 0.12, 0.08, 0.05, 0.035, 0.025);
    const float frequencies[6] = float[6](0.60, 1.20, 2.00, 3.50, 4.60, 5.80);
    const float speeds[6] = float[6](0.80, 1.10, 1.60, 2.20, 1.85, 2.55);
    const float phaseOffsets[6] = float[6](0.0, 1.7, 3.1, 4.6, 2.3, 5.4);
    float height = 0.0;
    for (int i = 0; i < 6; ++i) {
        float phase = dot(directions[i], xz) * frequencies[i] + speeds[i] * time + phaseOffsets[i];
        height += amplitudes[i] * sin(phase);
    }
    return waterBaseY + height * waveStrength;
}

vec2 refractedFloorHit(vec2 surfaceXZ, float targetY) {
    vec3 ray = refract(normalize(sunDirection), getWaterNormal(surfaceXZ), 1.0 / 1.333);
    if (dot(ray, ray) < 0.0001 || ray.y >= -0.001) return vec2(10000.0);
    return surfaceXZ + ray.xz * max(waveHeight(surfaceXZ) - targetY, 0.0) / -ray.y;
}

float snellCaustic(vec3 position) {
    vec2 surfaceXZ = position.xz;
    vec2 firstHit = refractedFloorHit(surfaceXZ, position.y);
    surfaceXZ += (position.xz - firstHit) * 0.72;

    float epsilon = 0.16;
    vec2 centerHit = refractedFloorHit(surfaceXZ, position.y);
    vec2 hitX = refractedFloorHit(surfaceXZ + vec2(epsilon, 0.0), position.y);
    vec2 hitZ = refractedFloorHit(surfaceXZ + vec2(0.0, epsilon), position.y);
    vec2 dx = (hitX - centerHit) / epsilon;
    vec2 dz = (hitZ - centerHit) / epsilon;
    float rayArea = abs(dx.x * dz.y - dx.y * dz.x);
    float concentration = clamp(1.0 / max(rayArea, 0.20), 0.0, 2.4);
    float physicalIntensity = smoothstep(0.65, 2.20, concentration);
    float hitAccuracy = exp(-dot(centerHit - position.xz, centerHit - position.xz) * 0.65);

    vec2 causticUv = surfaceXZ * 1.48 + getWaterNormal(surfaceXZ).xz * 0.28;
    causticUv += vec2(time * 0.0045, time * 0.0028);
    vec2 warpedUv = causticUv + vec2(
        sin(causticUv.y * 1.7 + time * 0.25),
        cos(causticUv.x * 1.3 - time * 0.20)
    ) * 0.08;
    float border = worleyBorder(warpedUv);
    float thinLine = 1.0 - smoothstep(0.010, 0.043, border);
    float softGlow = 1.0 - smoothstep(0.028, 0.125, border);
    float web = thinLine * 0.76 + softGlow * 0.24;
    float caustic = web * mix(0.25, 1.35, physicalIntensity) * hitAccuracy;
    caustic *= 0.85 + 0.15 * sin(time * 2.0 + concentration * 3.0);
    return clamp(caustic, 0.0, 1.20);
}

void main() {
    vec3 normal = normalize(worldNormal);
    float diffuse = max(dot(normal, -sunDirection), 0.0);
    vec3 materialColor = baseColor;
    if (shellDetail == 1) {
        float colorVariation = 0.94 + 0.06 * sin(localPos.x * 17.0 + localPos.z * 11.0);
        materialColor *= colorVariation;
    }
    vec3 color = materialColor * (vec3(0.54, 0.64, 0.60) + vec3(0.64, 0.68, 0.52) * diffuse);

    if (shellDetail == 1) {
        float cavityOcclusion = mix(0.68, 1.0, smoothstep(-0.15, 0.65, normal.y));
        float wornEdge = smoothstep(0.42, 0.93, abs(sin(localPos.x * 13.0 + localPos.z * 9.0)))
            * smoothstep(0.30, 0.85, length(localPos.xz));
        color *= cavityOcclusion;
        color += vec3(0.20, 0.16, 0.11) * wornEdge * 0.16;
    } else if (shellDetail == 2) {
        float branchOcclusion = mix(0.64, 1.0, smoothstep(-0.35, 0.75, normal.y));
        float baseOcclusion = mix(0.58, 1.0, smoothstep(0.03, 0.52, localPos.y));
        float organicVariation = 0.88
            + 0.07 * sin(localPos.y * 11.0 + localPos.x * 7.0)
            + 0.05 * sin(localPos.z * 15.0 - localPos.y * 4.0);
        vec3 viewDir = normalize(cameraPos - worldPos);
        vec3 halfDir = normalize(viewDir - sunDirection);
        float coralSpecular = pow(max(dot(normal, halfDir), 0.0), 28.0);
        color *= branchOcclusion * baseOcclusion * organicVariation;
        color += vec3(0.32, 0.48, 0.44) * coralSpecular * 0.13;
    }

    float distanceToCamera = length(worldPos - cameraPos);
    float lightPatch = 0.5 + 0.5 * sin(worldPos.x * 1.8 + worldPos.z * 1.5 + time * 0.08);
    color += vec3(0.34, 0.45, 0.26) * lightPatch * max(normal.y, 0.0) * 0.10;
    vec2 causticUv = worldPos.xz * 2.90 + getWaterNormal(worldPos.xz).xz * 0.30;
    causticUv += vec2(time * 0.0045, time * 0.0028);
    float caustic = 1.0 - smoothstep(0.018, 0.075, worleyBorder(causticUv));
    caustic = clamp(smoothstep(0.30, 0.86, caustic), 0.0, 0.88);
    if (causticsMode == 1 && shellDetail == 2) {
        // Coral highlights are driven by the refracted-ray concentration,
        // then shaped to stay readable across thin upper branches.
        float snellIntensity = snellCaustic(worldPos);
        caustic = pow(clamp(snellIntensity, 0.0, 1.2), 0.68);
    }
    float upwardMask = max(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0);
    float lightFacing = max(dot(normal, -sunDirection), 0.0);
    float surfaceMask = mix(0.10, 1.0, pow(upwardMask, 0.72)) * mix(0.62, 1.0, lightFacing);
    if (shellDetail == 2) {
        upwardMask = pow(max(upwardMask, 0.0), 0.58);
        surfaceMask = mix(0.26, 1.0, upwardMask) * mix(0.78, 1.0, lightFacing);
    }
    float objectFade = exp(-distanceToCamera * causticFade);
    if (shellDetail == 2) objectFade = exp(-distanceToCamera * causticFade * 0.72);
    caustic *= surfaceMask * objectFade;
    float selectedStrength = shellDetail == 2 ? coralCausticStrength : causticStrength;
    vec3 highlightColor = shellDetail == 2 ? vec3(1.0, 0.98, 0.84) : vec3(1.0, 0.98, 0.82);
    color += highlightColor * caustic * selectedStrength;
    if (shellDetail == 2 && coralCausticsDebug == 1) {
        fragColor = vec4(vec3(caustic), 1.0);
        return;
    }
    float fogFactor = clamp(1.0 - exp(-distanceToCamera * 0.06), 0.0, 1.0);
    color = mix(color, vec3(0.0, 0.52, 0.62), fogFactor);
    fragColor = vec4(color, 1.0);
}
