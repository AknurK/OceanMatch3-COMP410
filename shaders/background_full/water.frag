#version 330 core

in vec2 screenUv;

uniform float time;
uniform vec3 sunDirection;

out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    local = local * local * (3.0 - 2.0 * local);

    float a = hash(cell);
    float b = hash(cell + vec2(1.0, 0.0));
    float c = hash(cell + vec2(0.0, 1.0));
    float d = hash(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}

float softBeam(vec2 uv, float center, float slope, float width) {
    float beamCenter = center + slope * (1.0 - uv.y);
    float distanceFromBeam = abs(uv.x - beamCenter);
    return exp(-distanceFromBeam * distanceFromBeam / width);
}

void main() {
    vec3 topColor = vec3(0.06, 0.88, 0.94);
    vec3 midColor = vec3(0.0, 0.61, 0.68);
    vec3 deepColor = vec3(0.0, 0.27, 0.34);

    float lowerBlend = smoothstep(0.0, 0.48, screenUv.y);
    float upperBlend = smoothstep(0.42, 1.0, screenUv.y);
    vec3 waterColor = mix(deepColor, midColor, lowerBlend);
    waterColor = mix(waterColor, topColor, upperBlend);
    float horizonHaze = exp(-pow((screenUv.y - 0.43) / 0.25, 2.0));
    waterColor = mix(waterColor, vec3(0.01, 0.66, 0.72), horizonHaze * 0.72);

    vec2 surfaceUv = screenUv * vec2(7.0, 4.0);
    float surfaceNoise1 = noise(surfaceUv + vec2(time * 0.035, time * 0.018));
    float surfaceNoise2 = noise(surfaceUv * 1.8 + vec2(-time * 0.025, time * 0.03));
    float surfacePattern = smoothstep(0.58, 0.88, surfaceNoise1 * 0.62 + surfaceNoise2 * 0.38);
    float surfaceMask = smoothstep(0.48, 0.98, screenUv.y);
    waterColor += vec3(0.05, 0.14, 0.15) * surfacePattern * surfaceMask;

    vec2 sunScreenPosition = vec2(0.40 + sunDirection.x * 0.04, 1.02);
    vec2 sunDelta = (screenUv - sunScreenPosition) / vec2(0.58, 0.34);
    float sunGlow = exp(-dot(sunDelta, sunDelta) * 2.5);
    waterColor += vec3(0.42, 0.50, 0.42) * sunGlow;

    float scattering = noise(screenUv * vec2(5.0, 8.0) + time * 0.008);
    waterColor += vec3(0.0, 0.018, 0.022) * scattering;

    fragColor = vec4(waterColor, 1.0);
}
