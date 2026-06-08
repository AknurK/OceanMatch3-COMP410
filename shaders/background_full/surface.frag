#version 330 core

in vec3 worldPos;
in vec3 worldNormal;
in vec2 texCoord;

uniform sampler2D waterNormalTexture;
uniform vec3 cameraPos;
uniform vec3 sunDirection;
uniform float time;

out vec4 fragColor;

void main() {
    vec2 normalUv1 = texCoord * 0.32 + vec2(time * 0.022, time * 0.014);
    vec2 normalUv2 = texCoord * 0.43 + vec2(-time * 0.015, time * 0.019);
    vec2 distortion = texture(waterNormalTexture, normalUv1).rb * 2.0 - 1.0;
    distortion += (texture(waterNormalTexture, normalUv2).rb * 2.0 - 1.0) * 0.5;

    vec3 normalMap1 = texture(waterNormalTexture, normalUv1 + distortion * 0.035).rgb * 2.0 - 1.0;
    vec3 normalMap2 = texture(waterNormalTexture, normalUv2 - distortion * 0.025).rgb * 2.0 - 1.0;
    vec3 normalDetail = normalize(vec3(
        normalMap1.r + normalMap2.r * 0.65,
        1.0,
        normalMap1.b + normalMap2.b * 0.65
    ));
    vec3 normal = normalize(worldNormal + vec3(normalDetail.x, 0.0, normalDetail.z) * 0.32);
    vec3 viewDir = normalize(cameraPos - worldPos);
    if (dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }
    float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 3.0);
    vec3 sunDir = normalize(sunDirection);
    float spec = pow(max(dot(reflect(-sunDir, normal), viewDir), 0.0), 64.0);

    vec2 lightUv = texCoord * 0.18 + distortion * 0.12 + vec2(time * 0.01, -time * 0.008);
    float ripples1 = 0.5 + 0.5 * sin(lightUv.x * 5.0 + sin(lightUv.y * 4.0) + time * 0.35);
    float ripples2 = 0.5 + 0.5 * sin(lightUv.y * 4.3 - sin(lightUv.x * 3.2) - time * 0.26);
    float rippleField = ripples1 * 0.58 + ripples2 * 0.42;
    float surfaceLight = smoothstep(0.72, 0.98, rippleField);
    float softRippleLines = smoothstep(0.58, 0.82, rippleField) * 0.20;
    vec2 sunCenter = vec2(-1.5, -12.0) - sunDirection.xz * 4.0;
    float sunGlow = exp(-length(worldPos.xz - sunCenter) * 0.040);
    float distanceFade = exp(-length(worldPos - cameraPos) * 0.012);

    vec3 midTurquoise = vec3(0.0, 0.62, 0.70);
    vec3 shallowCyan = vec3(0.05, 0.90, 0.96);
    vec3 highlight = vec3(0.8, 1.0, 0.95);
    vec3 color = mix(midTurquoise, vec3(0.55, 1.0, 0.95), fresnel);
    color = mix(color, shallowCyan, sunGlow * 0.24);
    float waveFacing = 1.0 - abs(normal.y);
    color *= mix(0.82, 1.12, waveFacing);
    color += highlight * (softRippleLines * 1.55 + spec * 0.88);
    float alpha = (0.23 + waveFacing * 0.15 + fresnel * 0.13
        + surfaceLight * 0.055 + sunGlow * 0.070 + spec * 0.10)
        * distanceFade;
    fragColor = vec4(color, alpha);
}
