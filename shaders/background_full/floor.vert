#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 worldPos;
out vec3 worldNormal;
out vec2 texCoord;
out float seabedElevation;

vec2 gradientDirection(vec2 cell) {
    float angle = fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453) * 6.2831853;
    return vec2(cos(angle), sin(angle));
}

float gradientNoise(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    vec2 blend = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);

    float n00 = dot(gradientDirection(cell), local);
    float n10 = dot(gradientDirection(cell + vec2(1.0, 0.0)), local - vec2(1.0, 0.0));
    float n01 = dot(gradientDirection(cell + vec2(0.0, 1.0)), local - vec2(0.0, 1.0));
    float n11 = dot(gradientDirection(cell + vec2(1.0, 1.0)), local - vec2(1.0, 1.0));
    return mix(mix(n00, n10, blend.x), mix(n01, n11, blend.x), blend.y);
}

float roundedMounds(vec2 p) {
    vec2 scaled = p * 0.34;
    vec2 cell = floor(scaled);
    vec2 local = fract(scaled);
    float height = 0.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 id = cell + neighbor;
            vec2 center = vec2(
                fract(sin(dot(id, vec2(127.1, 311.7))) * 43758.5453),
                fract(sin(dot(id, vec2(269.5, 183.3))) * 43758.5453)
            );
            center = 0.18 + center * 0.64;
            float radius = mix(0.28, 0.48, fract(sin(dot(id, vec2(41.7, 289.1))) * 15731.743));
            float moundHeight = mix(0.10, 0.25, fract(sin(dot(id, vec2(93.3, 67.5))) * 24634.634));
            float distanceToCenter = length(local - neighbor - center);
            float mound = 1.0 - smoothstep(radius * 0.25, radius, distanceToCenter);
            mound *= mound;
            height = max(height, mound * moundHeight);
        }
    }
    return height;
}

float seabedHeight(vec2 p) {
    float broadDunes = gradientNoise(p * 0.040) * 0.16
        + gradientNoise(p * 0.075 + vec2(11.7, -4.3)) * 0.08;
    float smallMounds = roundedMounds(p);

    float rippleA = sin(p.x * 1.55 + p.y * 0.22 + sin(p.y * 0.12) * 0.8);
    float rippleB = sin(p.y * 1.28 - p.x * 0.16 + sin(p.x * 0.10) * 0.7);
    float ripples = rippleA * 0.032 + rippleB * 0.020;
    ripples *= 0.55 + 0.45 * (1.0 - smoothstep(4.0, 38.0, length(p)));

    float depressions = -exp(-dot(p - vec2(-1.5, -4.0), p - vec2(-1.5, -4.0)) * 0.045) * 0.24
        - exp(-dot(p - vec2(4.0, -10.5), p - vec2(4.0, -10.5)) * 0.060) * 0.19
        - exp(-dot(p - vec2(1.8, 1.5), p - vec2(1.8, 1.5)) * 0.12) * 0.16
        - exp(-dot(p - vec2(-4.0, 2.8), p - vec2(-4.0, 2.8)) * 0.18) * 0.11
        - exp(-dot(p - vec2(3.2, 3.4), p - vec2(3.2, 3.4)) * 0.22) * 0.10;
    float rockMounds = exp(-dot(p - vec2(-5.2, -2.0), p - vec2(-5.2, -2.0)) * 0.10) * 0.28
        + exp(-dot(p - vec2(5.8, -11.5), p - vec2(5.8, -11.5)) * 0.085) * 0.24
        + exp(-dot(p - vec2(5.0, 1.0), p - vec2(5.0, 1.0)) * 0.13) * 0.18
        + exp(-dot(p - vec2(-2.8, 3.2), p - vec2(-2.8, 3.2)) * 0.24) * 0.13
        + exp(-dot(p - vec2(2.7, 2.1), p - vec2(2.7, 2.1)) * 0.20) * 0.12;
    return broadDunes + smallMounds + ripples + depressions + rockMounds;
}

void main() {
    vec3 displaced = aPosition;
    float currentHeight = seabedHeight(aPosition.xz);
    displaced.y += currentHeight;
    seabedElevation = currentHeight;
    float epsilon = 0.18;
    float heightX = seabedHeight(aPosition.xz + vec2(epsilon, 0.0));
    float heightZ = seabedHeight(aPosition.xz + vec2(0.0, epsilon));
    vec3 displacedNormal = normalize(vec3(
        -(heightX - currentHeight) / epsilon,
        1.0,
        -(heightZ - currentHeight) / epsilon
    ));
    vec4 worldPosition = model * vec4(displaced, 1.0);
    worldPos = worldPosition.xyz;
    worldNormal = normalize(mat3(transpose(inverse(model))) * displacedNormal);
    texCoord = aTexCoord;
    gl_Position = projection * view * worldPosition;
}
