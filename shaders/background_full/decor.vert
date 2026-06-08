#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float coralVariation;
uniform int coralSoft;

out vec3 worldPos;
out vec3 worldNormal;
out vec3 localPos;

void main() {
    vec3 displacedPosition = aPosition;
    if (coralSoft == 1) {
        float tipWeight = smoothstep(0.10, 1.45, aPosition.y);
        displacedPosition.x += sin(time * 0.34 + aPosition.y * 1.65 + coralVariation) * tipWeight * 0.035;
        displacedPosition.z += cos(time * 0.27 + aPosition.y * 1.30 + coralVariation * 0.73) * tipWeight * 0.020;
    }
    vec4 position = model * vec4(displacedPosition, 1.0);
    worldPos = position.xyz;
    worldNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    localPos = aPosition;
    gl_Position = projection * view * position;
}
