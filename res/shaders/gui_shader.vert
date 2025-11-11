#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 parentModel;
    vec4 parentCornerRadius;
    vec4 parentMargin;
    vec4 margin;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec2 uvOffset;
layout(location = 3) out vec2 uvTiling;
layout(location = 4) out vec2 uv;
layout(location = 5) out vec2 worldPos;
layout(location = 6) out vec2 parentWorldPos;
layout(location = 7) out vec2 parentScale;
layout(location = 8) out vec4 parentCornerRadius;
layout(location = 9) out vec4 parentMargin;

void main() {
    vec4 pos = vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * pos;

    worldPos = (ubo.model * pos).xy;
    parentWorldPos = (ubo.parentModel * vec4(0.0, 0.0, 0.0, 1.0)).xy;
    parentScale = vec2(ubo.parentModel[0][0], ubo.parentModel[1][1]);
    parentCornerRadius = ubo.parentCornerRadius;
    parentMargin = ubo.parentMargin;

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    vec2 fragScale = vec2(ubo.model[0][0], ubo.model[1][1]);
    uvOffset = fragScale * 0.5;
    uvTiling = fragTexCoord * fragScale;
    uv = uvTiling - uvOffset;
}