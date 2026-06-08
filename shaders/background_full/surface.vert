#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform vec3 cameraPos;
uniform float waveStrength;
uniform float waterBaseY;

out vec3 worldPos;
out vec3 worldNormal;
out vec2 texCoord;

void evaluateWaves(vec2 xz, out float height, out vec2 derivative) {
    const vec2 directions[6] = vec2[6](
        vec2(0.957826, 0.287348),
        vec2(-0.514496, 0.857493),
        vec2(0.894427, -0.447214),
        vec2(-0.980581, -0.196116),
        vec2(0.316228, 0.948683),
        vec2(-0.707107, 0.707107)
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

void main() {
    float height;
    vec2 derivative;
    evaluateWaves(aPosition.xz, height, derivative);
    worldPos = vec3(aPosition.x, waterBaseY + height, aPosition.z);
    worldNormal = normalize(vec3(-derivative.x, 1.0, -derivative.y));
    texCoord = aTexCoord;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
