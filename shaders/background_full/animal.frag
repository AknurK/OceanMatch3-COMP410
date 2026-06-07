#version 330 core

in vec3 worldPos;
in vec3 worldNormal;
in vec3 localPos;

uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform int animalType;
uniform float time;
uniform float causticStrength;
uniform float causticFade;

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

float causticLight(vec2 p) {
    vec2 causticUv = p * 2.90 + vec2(time * 0.0045, time * 0.0028);
    float web = 1.0 - smoothstep(0.018, 0.075, worleyBorder(causticUv));
    return clamp(smoothstep(0.30, 0.86, web), 0.0, 0.88);
}

void main() {
    vec3 normal = normalize(worldNormal);
    float diffuse = max(dot(normal, -sunDirection), 0.0);

    vec3 baseColor;
    if (animalType == 0) {
        float whiteBand = step(0.18, abs(localPos.x)) * (1.0 - step(0.42, abs(localPos.x)));
        float blackEdge = step(0.14, abs(localPos.x)) * (1.0 - step(0.20, abs(localPos.x)));
        blackEdge += step(0.40, abs(localPos.x)) * (1.0 - step(0.47, abs(localPos.x)));
        baseColor = mix(vec3(0.78, 0.24, 0.055), vec3(0.82, 0.84, 0.78), whiteBand);
        baseColor = mix(baseColor, vec3(0.045, 0.055, 0.050), clamp(blackEdge, 0.0, 1.0));
    } else if (animalType == 1) {
        float underside = smoothstep(-0.4, 0.25, -normal.y);
        baseColor = mix(vec3(0.20, 0.34, 0.42), vec3(0.48, 0.62, 0.66), underside);
    } else if (animalType == 2) {
        float silverBand = 0.5 + 0.5 * smoothstep(-0.25, 0.30, normal.y);
        baseColor = mix(vec3(0.22, 0.36, 0.40), vec3(0.62, 0.73, 0.70), silverBand);
    } else if (animalType == 3) {
        float finShade = smoothstep(0.32, 0.58, abs(localPos.y));
        baseColor = mix(vec3(0.66, 0.52, 0.12), vec3(0.80, 0.68, 0.22), finShade);
    } else if (animalType == 5) {
        float yellowTail = 1.0 - smoothstep(-0.82, -0.64, localPos.x);
        float sidePattern = smoothstep(
            0.26,
            0.68,
            sin(localPos.x * 3.4 + localPos.y * 5.2) * 0.34
                + cos(localPos.x * 5.8 - localPos.y * 2.6) * 0.22
                + 0.42
        );
        float darkBack = smoothstep(0.08, 0.42, localPos.y + 0.10);
        vec3 oceanBlue = mix(vec3(0.025, 0.20, 0.62), vec3(0.06, 0.42, 0.88), diffuse * 0.60);
        vec3 darkPattern = vec3(0.012, 0.045, 0.15);
        baseColor = mix(oceanBlue, darkPattern, clamp(sidePattern * darkBack * 0.76, 0.0, 0.76));
        baseColor = mix(baseColor, vec3(1.0, 0.78, 0.04), yellowTail);
        float sideVisibility = smoothstep(0.35, 0.82, abs(normal.z));
        vec2 eyeUv = vec2((localPos.x - 0.57) / 0.16, (localPos.y - 0.20) / 0.14);
        float eyeWhite = (1.0 - smoothstep(0.68, 1.0, length(eyeUv))) * sideVisibility;
        float pupil = (1.0 - smoothstep(0.18, 0.42, length(eyeUv - vec2(0.12, 0.0)))) * sideVisibility;
        baseColor = mix(baseColor, vec3(0.78, 0.88, 0.90), eyeWhite * 0.82);
        baseColor = mix(baseColor, vec3(0.015, 0.018, 0.025), pupil);
    } else if (animalType == 6) {
        float mottling = 0.5 + 0.5 * sin(localPos.x * 8.0 + localPos.z * 7.0 + localPos.y * 4.0);
        baseColor = mix(vec3(0.38, 0.13, 0.26), vec3(0.62, 0.25, 0.32), mottling * 0.42);
    } else if (animalType == 7) {
        float shellMask = smoothstep(0.08, 0.24, localPos.y)
            * (1.0 - smoothstep(0.72, 1.05, length(localPos.xz)));
        float shellPattern = 0.5 + 0.5 * sin(localPos.x * 7.0) * cos(localPos.z * 8.0);
        vec3 skin = mix(vec3(0.18, 0.34, 0.25), vec3(0.30, 0.46, 0.30), diffuse);
        vec3 shell = mix(vec3(0.20, 0.28, 0.12), vec3(0.42, 0.48, 0.20), shellPattern * 0.42);
        baseColor = mix(skin, shell, shellMask);
    } else {
        float reefStripe = 0.5 + 0.5 * sin(localPos.x * 11.0);
        baseColor = mix(vec3(0.08, 0.28, 0.48), vec3(0.16, 0.52, 0.65), reefStripe * 0.35);
    }

    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 halfVector = normalize(viewDir - sunDirection);
    float specular = pow(max(dot(normal, halfVector), 0.0), 36.0);
    float distanceToCamera = length(worldPos - cameraPos);
    float upwardMask = max(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0);
    float lightFacing = max(dot(normal, -sunDirection), 0.0);
    float fishSurfaceMask = upwardMask * mix(0.35, 0.72, lightFacing);
    float caustic = causticLight(worldPos.xz)
        * fishSurfaceMask
        * exp(-distanceToCamera * causticFade);
    vec3 ambient = vec3(0.68, 0.74, 0.70);
    vec3 directional = vec3(0.48, 0.54, 0.48) * diffuse;
    vec3 color = baseColor * (ambient + directional);
    color += vec3(0.78, 1.0, 0.88) * specular * 0.28;
    color += vec3(1.0, 0.95, 0.75) * caustic * causticStrength;
    float fogFactor = clamp(1.0 - exp(-distanceToCamera * 0.06), 0.0, 1.0);
    vec3 attenuation = exp(-vec3(0.018, 0.008, 0.005) * distanceToCamera);
    color *= attenuation;
    color = mix(color, vec3(0.0, 0.52, 0.62), fogFactor * 0.68);
    fragColor = vec4(color, 1.0);
}
