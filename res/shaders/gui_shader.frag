#version 450

layout(location = 0) in vec4 fragColor; // RGBA, with alpha as a base value
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec2 uvOffset;
layout(location = 3) in vec2 uvTiling;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec2 worldPos;
layout(location = 6) in vec2 parentWorldPos;
layout(location = 7) in vec2 parentScale;
layout(location = 8) in vec4 parentCornerRadius;
layout(location = 9) in vec4 parentMargin;

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform GUIStyle {
    vec4 cornerRadius;       // x: top-left, y: top-right, z: bottom-left, w: bottom-right
    vec4 backgroundColor;
    vec4 borderThickness;   // x: top, y: right, z: bottom, w: left
    vec4 margin;
    vec4 borderTopColor;    // Color for top border
    vec4 borderRightColor;  // Color for right border
    vec4 borderBottomColor; // Color for bottom border
    vec4 borderLeftColor;   // Color for left border
} params;

layout(location = 0) out vec4 outColor;

const vec4 black = vec4(0.0, 0.0, 0.0, 1.0);
const vec4 white = vec4(1.0, 1.0, 1.0, 1.0);

// SDF for a rounded rectangle with per-corner radius
float roundedBoxSDF(vec2 centerPosition, vec2 size, vec4 radius) {
    float finalRadius = (centerPosition.x < 0.0 && centerPosition.y > 0.0) ? radius.x : // Top-left
    (centerPosition.x > 0.0 && centerPosition.y > 0.0) ? radius.y : // Top-right
    (centerPosition.x < 0.0 && centerPosition.y < 0.0) ? radius.z : // Bottom-left
    radius.w; // Bottom-right
    vec2 q = abs(centerPosition) - size + finalRadius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - finalRadius;
}

// Linear interpolation for vec3
vec3 lerp(vec3 colorA, vec3 colorB, float value) {
    return colorA + value * (colorB - colorA);
}

void main() {

    // Parent’s clipping mask, adjusted for margins
    vec2 parentCenter = parentWorldPos + parentScale * 0.5;
    vec2 parentSize = parentScale * 0.5 - vec2(parentMargin.y, parentMargin.x); // Reduce by right and top margins
    vec2 relativePos = worldPos - parentCenter;
    float parentDistance = roundedBoxSDF(relativePos, parentSize, parentCornerRadius);
    float parentMask = smoothstep(-0.5, 0.0, parentDistance);

    vec2 rectCorner = uvOffset - vec2(params.margin.w + params.margin.y, params.margin.x + params.margin.z) * 0.5;
    vec2 finalUV = uv - vec2(params.margin.w - params.margin.y, params.margin.x - params.margin.z)*0.5;

    // Compute border corner with optimized quadrant check
    vec2 borderCorner = rectCorner;
    float yQuad = step(uvOffset.y, uvTiling.y); // 0 if above center, 1 if below
    float xQuad = step(uvOffset.x, uvTiling.x); // 0 if left of center, 1 if right
    borderCorner.y -= mix(params.borderThickness.x, params.borderThickness.z, yQuad); // Top or bottom thickness
    borderCorner.x -= mix(params.borderThickness.w, params.borderThickness.y, xQuad); // Left or right thickness

    // Adjust inner corner radius based on border thickness
    vec4 adjustedCornerRadius = params.cornerRadius;
    float maxThicknessX = mix(params.borderThickness.w, params.borderThickness.y, xQuad); // Left or right max
    float maxThicknessY = mix(params.borderThickness.x, params.borderThickness.z, yQuad); // Top or bottom max
    adjustedCornerRadius.x = max(params.cornerRadius.x - (maxThicknessX + maxThicknessY) * 0.5, 0.0); // Top-left
    adjustedCornerRadius.y = max(params.cornerRadius.y - (maxThicknessX + maxThicknessY) * 0.5, 0.0); // Top-right
    adjustedCornerRadius.z = max(params.cornerRadius.z - (maxThicknessX + maxThicknessY) * 0.5, 0.0); // Bottom-left
    adjustedCornerRadius.w = max(params.cornerRadius.w - (maxThicknessX + maxThicknessY) * 0.5, 0.0); // Bottom-right

    // Compute SDF distances
    float shapeDistance = roundedBoxSDF(finalUV, rectCorner, params.cornerRadius);
    float backgroundDistance = roundedBoxSDF(finalUV, borderCorner, adjustedCornerRadius);

    // Determine the closest edge color for the entire shape
    vec4 borderColor = params.borderTopColor; // Default to top color as fallback
    float topDist = abs(uvTiling.y - (uvOffset.y + rectCorner.y));
    float rightDist = abs(uvTiling.x - (uvOffset.x + rectCorner.x));
    float bottomDist = abs(uvTiling.y - (uvOffset.y - rectCorner.y));
    float leftDist = abs(uvTiling.x - (uvOffset.x - rectCorner.x));
    float minDist = min(min(topDist, rightDist), min(bottomDist, leftDist));

    if (minDist == topDist) {
        borderColor = params.borderTopColor;
    } else if (minDist == rightDist) {
        borderColor = params.borderRightColor;
    } else if (minDist == bottomDist) {
        borderColor = params.borderBottomColor;
    } else if (minDist == leftDist) {
        borderColor = params.borderLeftColor;
    }

    // Create the mask using shapeDistance and backgroundDistance transitions
    vec4 marginMask = white * smoothstep(-0.5, 0.0, backgroundDistance);
    vec4 backgroundMask = white * smoothstep(-0.5, 0.0, shapeDistance);

    // Combine Fragcolor with textColor
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec4 mainColorOpacity = vec4(lerp(black.rgb, white.rgb, params.backgroundColor.a), 1.0);
    float mainMask = mix(mainColorOpacity, black, backgroundMask).r;
    float borderMask = mix(black, white, marginMask).r;
    float invertMainMask = mix(white, black, backgroundMask).r;
    float finalBorderMask = borderMask * invertMainMask;

    // Coloring
    vec4 finalBorderColor = borderMask * borderColor;
    vec4 finalMainColor = mainMask * texColor * params.backgroundColor;
    vec4 finalColor = mix(finalMainColor, finalBorderColor, finalBorderMask);

    vec2 size = vec2(100.0f, 100.0f) * 0.5;
    vec2 location = vec2(0.0f, 0.0f);
    float radius = 7.0f;

    float distance = roundedBoxSDF(vec2(50.0,50.0)*0.5, size, vec4(radius));

    float smoothedAlpha = smoothstep(-0.5f, 0.0, distance);

    vec3 color = mix(vec3(1), vec3(0), smoothedAlpha);

    vec4 parentCut = vec4(color, 1.0);


    outColor = vec4(finalColor.rgb, finalColor.a * parentCut.r);
}