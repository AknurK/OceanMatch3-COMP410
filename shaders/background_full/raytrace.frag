#version 330 core

in vec2 screenUv;

uniform mat4 inverseViewProjection;
uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform float time;

out vec4 fragColor;

const float INF = 1e20;

struct Hit {
    float t;
    vec3 position;
    vec3 normal;
    vec3 color;
    float reflectivity;
    float transparency;
};

float sphereIntersect(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return INF;
    h = sqrt(h);
    float nearT = -b - h;
    float farT = -b + h;
    return nearT > 0.001 ? nearT : (farT > 0.001 ? farT : INF);
}

float planeIntersect(vec3 ro, vec3 rd, float height) {
    if (abs(rd.y) < 0.0001) return INF;
    float t = (height - ro.y) / rd.y;
    return t > 0.001 ? t : INF;
}

void testSphere(
    inout Hit hit,
    vec3 ro,
    vec3 rd,
    vec3 center,
    float radius,
    vec3 color,
    float reflectivity,
    float transparency
) {
    float t = sphereIntersect(ro, rd, center, radius);
    if (t < hit.t) {
        hit.t = t;
        hit.position = ro + rd * t;
        hit.normal = normalize(hit.position - center);
        hit.color = color;
        hit.reflectivity = reflectivity;
        hit.transparency = transparency;
    }
}

Hit traceScene(vec3 ro, vec3 rd) {
    Hit hit;
    hit.t = INF;
    hit.position = vec3(0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.color = vec3(0.0);
    hit.reflectivity = 0.0;
    hit.transparency = 0.0;

    testSphere(hit, ro, rd, vec3(-2.5, 1.1, -5.0), 1.1, vec3(0.82, 0.28, 0.10), 0.18, 0.0);
    testSphere(hit, ro, rd, vec3(0.2, 1.35, -7.0), 1.35, vec3(0.08, 0.50, 0.68), 0.12, 0.72);
    testSphere(hit, ro, rd, vec3(3.1, 0.85, -5.8), 0.85, vec3(0.74, 0.62, 0.16), 0.62, 0.0);
    testSphere(hit, ro, rd, vec3(1.8, 2.8, -10.0), 1.2, vec3(0.20, 0.32, 0.62), 0.28, 0.0);

    float floorT = planeIntersect(ro, rd, 0.0);
    if (floorT < hit.t) {
        hit.t = floorT;
        hit.position = ro + rd * floorT;
        hit.normal = vec3(0.0, 1.0, 0.0);
        float sandNoise = 0.5 + 0.5 * sin(hit.position.x * 2.7) * sin(hit.position.z * 2.3);
        hit.color = mix(vec3(0.43, 0.40, 0.25), vec3(0.66, 0.61, 0.38), sandNoise * 0.35);
        hit.reflectivity = 0.06;
        hit.transparency = 0.0;
    }
    return hit;
}

float shadowTrace(vec3 position, vec3 lightDir) {
    vec3 ro = position + lightDir * 0.02;
    float shadow = 1.0;
    for (int i = -1; i <= 1; ++i) {
        vec3 jitter = vec3(float(i) * 0.025, 0.0, sin(float(i) * 2.4) * 0.025);
        Hit blocker = traceScene(ro, normalize(lightDir + jitter));
        shadow -= blocker.t < INF ? 0.23 : 0.0;
    }
    return clamp(shadow, 0.28, 1.0);
}

vec3 waterBackground(vec3 rd) {
    float vertical = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 deep = vec3(0.0, 0.18, 0.27);
    vec3 shallow = vec3(0.0, 0.72, 0.80);
    float rays = pow(max(dot(rd, normalize(vec3(-0.18, 0.86, -0.46))), 0.0), 22.0);
    return mix(deep, shallow, vertical) + vec3(0.42, 0.74, 0.72) * rays * 0.22;
}

vec3 directLighting(Hit hit, vec3 viewDir) {
    vec3 lightDir = normalize(-sunDirection);
    float diffuse = max(dot(hit.normal, lightDir), 0.0);
    float shadow = shadowTrace(hit.position, lightDir);
    vec3 halfVector = normalize(lightDir + viewDir);
    float specular = pow(max(dot(hit.normal, halfVector), 0.0), 64.0);
    vec3 color = hit.color * (vec3(0.18, 0.30, 0.30) + diffuse * shadow * vec3(0.72, 0.78, 0.58));
    color += vec3(0.72, 0.92, 0.88) * specular * shadow * 0.52;
    return color;
}

vec3 shadeSecondaryRay(vec3 rayOrigin, vec3 rayDirection) {
    Hit secondary = traceScene(rayOrigin, rayDirection);
    if (secondary.t >= INF) return waterBackground(rayDirection);

    vec3 color = directLighting(secondary, normalize(-rayDirection));
    if (secondary.reflectivity > 0.0) {
        vec3 bounceDirection = reflect(rayDirection, secondary.normal);
        Hit bounce = traceScene(secondary.position + secondary.normal * 0.025, bounceDirection);
        vec3 bounceColor = bounce.t < INF
            ? directLighting(bounce, normalize(-bounceDirection))
            : waterBackground(bounceDirection);
        color = mix(color, bounceColor, secondary.reflectivity * 0.55);
    }
    return color;
}

vec3 shadeHit(Hit hit, vec3 ro, vec3 rd) {
    vec3 viewDir = normalize(ro - hit.position);
    vec3 color = directLighting(hit, viewDir);

    if (hit.reflectivity > 0.0) {
        vec3 reflectedDir = reflect(rd, hit.normal);
        vec3 reflectedColor = shadeSecondaryRay(hit.position + hit.normal * 0.025, reflectedDir);
        color = mix(color, reflectedColor, hit.reflectivity);
    }

    if (hit.transparency > 0.0) {
        const float airToGlass = 1.0 / 1.333;
        vec3 insideDir = refract(rd, hit.normal, airToGlass);
        vec3 refractedColor = waterBackground(rd);

        if (dot(insideDir, insideDir) > 0.001) {
            // Trace through the transparent sphere and refract again at the exit surface.
            Hit exitHit = traceScene(hit.position - hit.normal * 0.035, insideDir);
            if (exitHit.t < INF && exitHit.transparency > 0.0) {
                vec3 exitDir = refract(insideDir, -exitHit.normal, 1.333);
                if (dot(exitDir, exitDir) > 0.001) {
                    refractedColor = shadeSecondaryRay(exitHit.position + exitDir * 0.035, exitDir);
                } else {
                    vec3 internalReflection = reflect(insideDir, -exitHit.normal);
                    refractedColor = shadeSecondaryRay(
                        exitHit.position + internalReflection * 0.035,
                        internalReflection
                    );
                }
            } else if (exitHit.t < INF) {
                refractedColor = directLighting(exitHit, normalize(-insideDir));
            }
        }

        float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(viewDir, hit.normal), 0.0), 5.0);
        vec3 fresnelReflection = shadeSecondaryRay(
            hit.position + hit.normal * 0.025,
            reflect(rd, hit.normal)
        );
        vec3 glassTransport = mix(refractedColor * vec3(0.72, 0.94, 0.98), fresnelReflection, fresnel);
        color = mix(color, glassTransport, hit.transparency);
    }

    float fog = 1.0 - exp(-hit.t * 0.045);
    return mix(color, vec3(0.0, 0.46, 0.56), fog);
}

void main() {
    vec2 ndc = screenUv * 2.0 - 1.0;
    vec4 nearPoint = inverseViewProjection * vec4(ndc, -1.0, 1.0);
    vec4 farPoint = inverseViewProjection * vec4(ndc, 1.0, 1.0);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    vec3 rayOrigin = cameraPos;
    vec3 rayDirection = normalize(farPoint.xyz - nearPoint.xyz);
    Hit primary = traceScene(rayOrigin, rayDirection);
    vec3 color = primary.t < INF ? shadeHit(primary, rayOrigin, rayDirection) : waterBackground(rayDirection);

    float vignette = 1.0 - smoothstep(0.55, 1.45, length(ndc));
    color *= mix(0.72, 1.0, vignette);
    fragColor = vec4(color, 1.0);
}
