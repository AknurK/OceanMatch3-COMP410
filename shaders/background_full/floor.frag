#version 330 core

in vec3 worldPos;
in vec3 worldNormal;
in vec2 texCoord;
in float seabedElevation;

uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform float time;
uniform sampler2D sandTexture;
uniform int causticsMode;
uniform int causticsDebug;
uniform float waterBaseY;
uniform float causticStrength;
uniform float causticFade;
uniform float waveStrength;

out vec4 fragColor;

vec2 hash22(vec2 p) {
    vec2 q = vec2(
        dot(p, vec2(127.1, 311.7)),
        dot(p, vec2(269.5, 183.3))
    );
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
            vec2 randomPoint = hash22(cell + offset);
            vec2 point = 0.5 + 0.32 * sin(time * 0.045 + 6.2831 * randomPoint);
            vec2 difference = offset + point - local;
            float distanceSquared = dot(difference, difference);
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

void evaluateWaves(vec2 xz, out float height, out vec2 derivative) {
    const vec2 directions[6] = vec2[6](
        vec2(0.957826, 0.287348), vec2(-0.514496, 0.857493),
        vec2(0.894427, -0.447214), vec2(-0.980581, -0.196116),
        vec2(0.316228, 0.948683), vec2(-0.707107, 0.707107)
    );
    const float amplitudes[6] = float[6](0.20, 0.12, 0.08, 0.05, 0.035, 0.025);
    const float frequencies[6] = float[6](0.60, 1.20, 2.00, 3.50, 4.60, 5.80);
    const float speeds[6] = float[6](0.80, 1.10, 1.60, 2.20, 1.85, 2.55);
    const float phaseOffsets[6] = float[6](0.0, 1.7, 3.1, 4.6, 2.3, 5.4);
    height = 0.0;
    derivative = vec2(0.0);
    for (int i = 0; i < 6; ++i) {
        float phase = dot(directions[i], xz) * frequencies[i] + speeds[i] * time + phaseOffsets[i];
        height += amplitudes[i] * sin(phase);
        derivative += amplitudes[i] * frequencies[i] * directions[i] * cos(phase);
    }
    height *= waveStrength;
    derivative *= waveStrength;
}

float waveHeight(vec2 xz) {
    float height;
    vec2 derivative;
    evaluateWaves(xz, height, derivative);
    return waterBaseY + height;
}

vec3 getWaterNormal(vec2 xz) {
    float height;
    vec2 derivative;
    evaluateWaves(xz, height, derivative);
    return normalize(vec3(-derivative.x, 1.0, -derivative.y));
}

vec2 refractedFloorHit(vec2 surfaceXZ, float floorY) {
    vec3 surfaceNormal = getWaterNormal(surfaceXZ);
    vec3 incomingSunRay = normalize(sunDirection);
    vec3 refractedRay = refract(incomingSunRay, surfaceNormal, 1.0 / 1.333);
    if (dot(refractedRay, refractedRay) < 0.0001 || refractedRay.y >= -0.001) {
        return vec2(10000.0);
    }

    float depth = max(waveHeight(surfaceXZ) - floorY, 0.0);
    return surfaceXZ + refractedRay.xz * depth / -refractedRay.y;
}

float snellCaustic(vec3 fragmentPosition) {
    // Reverse-map this seabed fragment to an approximate point on the wave surface.
    vec2 surfaceXZ = fragmentPosition.xz;
    vec2 firstHit = refractedFloorHit(surfaceXZ, fragmentPosition.y);
    surfaceXZ += (fragmentPosition.xz - firstHit) * 0.72;

    // The Jacobian of nearby refracted rays estimates light concentration.
    float epsilon = 0.16;
    vec2 centerHit = refractedFloorHit(surfaceXZ, fragmentPosition.y);
    vec2 hitX = refractedFloorHit(surfaceXZ + vec2(epsilon, 0.0), fragmentPosition.y);
    vec2 hitZ = refractedFloorHit(surfaceXZ + vec2(0.0, epsilon), fragmentPosition.y);
    vec2 dx = (hitX - centerHit) / epsilon;
    vec2 dz = (hitZ - centerHit) / epsilon;
    float rayArea = abs(dx.x * dz.y - dx.y * dz.x);
    float concentration = clamp(1.0 / max(rayArea, 0.20), 0.0, 2.4);
    float physicalIntensity = smoothstep(0.65, 2.20, concentration);
    float hitAccuracy = exp(-dot(centerHit - fragmentPosition.xz, centerHit - fragmentPosition.xz) * 0.65);

    // The web gives focused light a readable shape, but Snell concentration controls brightness.
    vec2 causticUv = surfaceXZ * 1.48 + getWaterNormal(surfaceXZ).xz * 0.28;
    causticUv += vec2(time * 0.0045, time * 0.0028);
    vec2 warpedUv = causticUv + vec2(
        sin(causticUv.y * 1.7 + time * 0.25),
        cos(causticUv.x * 1.3 - time * 0.20)
    ) * 0.08;
    float border = worleyBorder(warpedUv);
    float thinLine = 1.0 - smoothstep(0.010, 0.046, border);
    float softGlow = 1.0 - smoothstep(0.028, 0.135, border);
    float web = thinLine * 0.82 + softGlow * 0.30;
    float caustic = web * mix(0.25, 1.35, physicalIntensity) * hitAccuracy;
    caustic *= 0.85 + 0.15 * sin(time * 2.0 + concentration * 3.0);
    return clamp(caustic, 0.0, 1.20);
}

void main() {
    vec3 waterColor = vec3(0.0, 0.61, 0.68);

    float distanceToCamera = length(worldPos - cameraPos);
    float fogDensity = 0.052;
    float fogDistance = 1.0 - exp(-distanceToCamera * fogDensity);
    float fogHeight = smoothstep(-5.0, 10.0, worldPos.y);
    float atmosphericScatter = smoothstep(8.0, 52.0, distanceToCamera);
    float fogFactor = 1.0
        - (1.0 - fogDistance)
        * (1.0 - fogHeight * 0.12)
        * (1.0 - atmosphericScatter * 0.86);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    float upwardFacing = clamp(worldNormal.y, 0.0, 1.0);
    vec3 sandColor = texture(sandTexture, texCoord).rgb;
    vec3 normal = normalize(worldNormal);
    float diffuse = max(dot(normal, -sunDirection), 0.0);
    vec3 ambientLight = vec3(0.42, 0.52, 0.46);
    vec3 sunLight = vec3(0.72, 0.78, 0.58) * diffuse;
    vec3 baseColor = sandColor * (ambientLight + sunLight) * mix(0.92, 1.0, upwardFacing);
    baseColor = mix(baseColor, vec3(0.42, 0.50, 0.35), 0.12);
    float moundGradient = smoothstep(-0.12, 0.24, seabedElevation);
    float slopeShade = smoothstep(0.72, 0.98, upwardFacing);
    baseColor *= mix(0.84, 1.12, moundGradient);
    baseColor *= mix(0.88, 1.04, slopeShade);

    vec3 waterNormal = getWaterNormal(worldPos.xz);
    vec2 causticUv = worldPos.xz * 2.90;
    causticUv += waterNormal.xz * 0.34;
    causticUv += vec2(time * 0.0045, time * 0.0028);
    float border = worleyBorder(causticUv);
    float causticPattern = 1.0 - smoothstep(0.018, 0.075, border);
    causticPattern = clamp(smoothstep(0.30, 0.86, causticPattern), 0.0, 0.88);

    float causticDistanceFade = exp(-distanceToCamera * causticFade) * (1.0 - fogFactor);
    float sunTint = clamp(dot(normalize(worldNormal), -sunDirection), 0.0, 1.0);
    vec2 sunFootprint = vec2(-5.0, -12.0) - sunDirection.xz * 14.0;
    float directSun = exp(-length(worldPos.xz - sunFootprint) * 0.045);
    float broadLightVariation = 0.5
        + 0.5 * sin(worldPos.x * 0.032 + time * 0.010)
        * sin(worldPos.z * 0.026 - time * 0.008);
    float secondaryVariation = 0.5
        + 0.5 * sin((worldPos.x + worldPos.z) * 0.018 + time * 0.006);
    float localLight = mix(0.18, 1.0, broadLightVariation * 0.72 + secondaryVariation * 0.28);
    localLight *= mix(0.52, 1.42, directSun);
    float selectedCaustic = causticsMode == 1 ? snellCaustic(worldPos) : causticPattern * localLight;
    selectedCaustic *= upwardFacing;
    selectedCaustic = clamp(selectedCaustic, 0.0, causticsMode == 1 ? 1.20 : 0.88);
    float causticGlow = pow(selectedCaustic, 0.72) * 0.13;
    vec3 causticColor = vec3(1.0, 0.97, 0.82)
        * selectedCaustic * causticStrength * causticDistanceFade;
    vec3 glowColor = vec3(0.82, 0.94, 0.72) * causticGlow * causticDistanceFade;
    baseColor += (causticColor + glowColor) * mix(0.85, 1.0, sunTint);

    if (causticsDebug == 1) {
        fragColor = vec4(vec3(selectedCaustic * causticDistanceFade), 1.0);
        return;
    }

    vec3 color = mix(baseColor, waterColor, fogFactor);

    fragColor = vec4(color, 1.0);
}
