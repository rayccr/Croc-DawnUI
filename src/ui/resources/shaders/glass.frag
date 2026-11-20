#version 440
// 仅折射应用环境纹理，不变形文字，也不截取其他窗口。
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 canvasSize;
    vec4 sourceBounds;
    vec4 shapeInfo;
    vec4 tintInfo;
    vec4 accentInfo;
    vec4 edgeInfo;
    vec4 lightInfo;
};
layout(binding = 1) uniform sampler2D source;

float roundedBox(vec2 point, vec2 halfSize, float radius) {
    vec2 delta = abs(point) - halfSize + radius;
    return length(max(delta, 0.0)) + min(max(delta.x, delta.y), 0.0) - radius;
}
vec3 blurred(vec2 uv) {
    vec2 stepSize = vec2(4.5) / canvasSize;
    vec3 color = texture(source, clamp(uv, 0.0, 1.0)).rgb * 0.28;
    color += texture(source, clamp(uv + vec2(stepSize.x, 0), 0.0, 1.0)).rgb * 0.12;
    color += texture(source, clamp(uv - vec2(stepSize.x, 0), 0.0, 1.0)).rgb * 0.12;
    color += texture(source, clamp(uv + vec2(0, stepSize.y), 0.0, 1.0)).rgb * 0.12;
    color += texture(source, clamp(uv - vec2(0, stepSize.y), 0.0, 1.0)).rgb * 0.12;
    color += texture(source, clamp(uv + stepSize, 0.0, 1.0)).rgb * 0.06;
    color += texture(source, clamp(uv - stepSize, 0.0, 1.0)).rgb * 0.06;
    color += texture(source, clamp(uv + vec2(stepSize.x, -stepSize.y), 0.0, 1.0)).rgb * 0.06;
    color += texture(source, clamp(uv + vec2(-stepSize.x, stepSize.y), 0.0, 1.0)).rgb * 0.06;
    return color;
}
void main() {
    vec2 size = max(shapeInfo.xy, vec2(1.0));
    vec2 pixel = qt_TexCoord0 * size;
    vec2 centered = pixel - size * 0.5;
    float radius = min(shapeInfo.z, min(size.x, size.y) * 0.5);
    float distance = roundedBox(centered, size * 0.5 - 0.7, radius);
    float mask = 1.0 - smoothstep(-0.7, 0.7, distance);
    vec2 normal = normalize(vec2(
        roundedBox(centered + vec2(0.5, 0), size * 0.5 - 0.7, radius) - distance,
        roundedBox(centered + vec2(0, 0.5), size * 0.5 - 0.7, radius) - distance) + vec2(0.0001));
    float rim = exp(-abs(distance) / 9.0);
    vec2 lightDelta = (pixel - lightInfo.xy) / max(size, vec2(180));
    vec2 bend = normal * rim * (5.0 + lightInfo.z * 2.5) + lightDelta * lightInfo.z * 1.4;
    vec2 uv = (sourceBounds.xy + pixel + bend) / canvasSize;
    vec3 color = mix(blurred(uv), tintInfo.rgb, tintInfo.a);
    // 表面冷色薄膜；高光只在指针附近和边缘变化，没有循环扫光。
    float topLight = pow(1.0 - qt_TexCoord0.y, 3.0);
    float reflection = exp(-dot(lightDelta, lightDelta) * 5.0);
    color += vec3(0.022, 0.028, 0.038) * topLight;
    color += mix(vec3(0.65, 0.78, 0.95), accentInfo.rgb, 0.25)
             * reflection * (0.012 + 0.028 * lightInfo.z);
    color += accentInfo.rgb * rim * 0.014;
    float edge = 1.0 - smoothstep(0.3, 1.6, abs(distance + 0.7));
    vec2 lightDirection = normalize(lightInfo.xy - pixel + vec2(0.001));
    float specular = max(dot(normal, lightDirection), 0.0);
    color = mix(color, edgeInfo.rgb, edge * edgeInfo.a * 0.32);
    color += vec3(0.72, 0.84, 1.0) * edge * (0.045 + specular * (0.11 + lightInfo.z * 0.11));
    float alpha = mask * shapeInfo.w * qt_Opacity;
    fragColor = vec4(color * alpha, alpha);
}
