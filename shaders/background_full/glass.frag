#version 330 core

in vec3 worldPos;
in vec3 worldNormal;
in vec3 localPos;

uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform float time;

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

void main() {
    vec3 normal = normalize(worldNormal);
    vec3 viewDir = normalize(cameraPos - worldPos);
    if (dot(normal, viewDir) < 0.0) normal = -normal;
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.3);
    vec3 halfDir = normalize(viewDir - sunDirection);
    float specular = pow(max(dot(normal, halfDir), 0.0), 86.0);
    vec3 color = mix(vec3(0.045, 0.22, 0.18), vec3(0.24, 0.76, 0.64), fresnel);

    vec2 causticUv = worldPos.xz * 2.90 + vec2(time * 0.0045, time * 0.0028);
    float caustic = 1.0 - smoothstep(0.018, 0.075, worleyBorder(causticUv));
    caustic = smoothstep(0.42, 0.90, caustic);
    float upwardMask = max(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0);
    color += vec3(0.74, 1.0, 0.90) * caustic * upwardMask * 0.10;
    color += vec3(0.82, 1.0, 0.94) * specular * 1.10;
    color += vec3(0.18, 0.62, 0.52) * fresnel * 0.30;

    float distanceToCamera = length(worldPos - cameraPos);
    float fog = clamp(1.0 - exp(-distanceToCamera * 0.055), 0.0, 1.0);
    color = mix(color, vec3(0.0, 0.61, 0.68), fog * 0.60);
    fragColor = vec4(color, mix(0.25, 0.45, fresnel));
}
