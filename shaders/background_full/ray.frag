#version 330 core

in vec3 worldPos;
in vec2 texCoord;

uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform float time;
uniform float rayOffset;

out vec4 fragColor;

void main() {
    float horizontal = exp(-pow((texCoord.x - 0.5) / 0.42, 2.0));
    float vertical = smoothstep(0.0, 0.20, texCoord.y)
        * (1.0 - smoothstep(0.68, 1.0, texCoord.y));
    float shimmer = 0.94 + 0.06 * sin(time * 0.20 + rayOffset);
    float fogFade = exp(-length(worldPos - cameraPos) * 0.055);
    float depthFade = smoothstep(0.0, 5.0, worldPos.y);
    float depthBelowSurface = max(10.0 - worldPos.y, 0.0);
    float sunlightFalloff = exp(-depthBelowSurface * 0.18);
    float sunAlignment = 0.85 + 0.15 * clamp(-sunDirection.y, 0.0, 1.0);
    float alpha = horizontal * vertical * shimmer * fogFade * depthFade
        * sunlightFalloff * sunAlignment * 0.25;
    fragColor = vec4(vec3(0.84, 1.0, 0.98), alpha);
}
