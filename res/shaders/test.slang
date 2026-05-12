struct VSInput {
    float3 inPos;
    float3 inNormal;
    float2 inTexCoord;
    float3 inColor;
};

struct UniformBuffer {
    int32_t  parentID;
    uint32_t displayOverflowMode;
    uint32_t imageID;
    uint32_t imageRepeatMode;
    float4x4 model;
    float4   imageOffsetAndTiling;
    float4   imageColor;
    float4   backgroundColor;
    float4   borderBottomColor;
    float4   borderTopColor;
    float4   borderLeftColor;
    float4   borderRightColor;
    float4   cornerRadius;
    float4   borderThickness;
    float4   margin;
};

struct ScreenBuffer {
    float4x4 viewProjection;
};

[[vk::binding(0, 0)]] StructuredBuffer<UniformBuffer> uniformBuffer;
[[vk::binding(1, 0)]] ConstantBuffer<ScreenBuffer>    sb;

struct VSOutput {
    float4      pos : SV_Position;
    //Texture
    float2      imageUV;
    uint        imageID;
    uint        imageRepeatMode;
    //Colors
    float4      bgColor;
    float4      imageColor;
    float4      borderBottomColor;
    float4      borderTopColor;
    float4      borderLeftColor;
    float4      borderRightColor;
    //Outer Shape
    float4      outerRadius;
    float2      outerSize;
    float2      outerP;
    //Inner Shape
    float4      innerRadius;
    float2      innerSize;
    float2      innerP;
    //Border
    float4      borderThickness;
};

const static float4 black = float4(0.0, 0.0, 0.0, 1.0);
const static float4 white = float4(1.0, 1.0, 1.0, 1.0);

float3 lerp2(float3 colorA, float3 colorB, float1 value) {
    return colorA + value * (colorB - colorA);
}



float1 sdRoundedBox(float2 p, float2 size, float4 r,) {
    float radius;
        if (p.x > 0.0) {
            radius = (p.y > 0.0) ? r.x : r.y;
        } else {
            radius = (p.y > 0.0) ? r.z : r.w;
        }

    float2 q = abs(p) - size + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;

}

float4 adjustedInnerRadius(float2 size, float2 offset, float2 tiling,float4 b, float4 r) {
    float1 yQuad = step(tiling.y, offset.y); // 0 if above center, 1 if below
    float1 xQuad = step(tiling.x, offset.x); // 0 if left of center, 1 if right

    // Adjust inner corner radius based on border thickness
    float4 adjustedCornerRadius = r;
    float1 maxThicknessX = lerp(b.y, b.w, xQuad); // Left or right max
    float1 maxThicknessY = lerp(b.x, b.z, yQuad); // Top or bottom max
    float1 maxThickness = (maxThicknessX + maxThicknessY) * 0.5;
    adjustedCornerRadius.x = max(r.x - maxThickness, 0.0); // Top-left
    adjustedCornerRadius.y = max(r.y - maxThickness, 0.0); // Top-right
    adjustedCornerRadius.z = max(r.z - maxThickness, 0.0); // Bottom-left
    adjustedCornerRadius.w = max(r.w - maxThickness, 0.0); // Bottom-right
    return adjustedCornerRadius;
}

float4 selectBorderColor(
    float2 outerP, float2 outerSize,
    float2 innerP, float2 innerSize, float4 innerRadii,
    float4 topColor, float4 bottomColor, float4 leftColor, float4 rightColor
) {
    float2 p = outerP;
    float2 s = sign(p);

    // 1. Select the Inner Radius for the current quadrant
    // Mapping based on your UniformBuffer: x:BR, y:TR, z:BL, w:TL
    float iR = (p.x > 0.0) ? (p.y > 0.0 ? innerRadii.x : innerRadii.y)  // TR : BR
                           : (p.y > 0.0 ? innerRadii.z : innerRadii.w); // TL : BL

    // 2. Calculate local thicknesses for the miter math
    // This defines the trapezoid shapes for the linear miter
    float2 t;
    t.x = (s.x > 0.0) ? (outerSize.x - innerSize.x - (outerP.x - innerP.x))  // Right
                      : (outerSize.x - innerSize.x + (outerP.x - innerP.x)); // Left
    t.y = (s.y > 0.0) ? (outerSize.y - innerSize.y - (outerP.y - innerP.y))  // Top
                      : (outerSize.y - innerSize.y + (outerP.y - innerP.y)); // Bottom

    // 3. The Logic Switch
    if (iR <= 0.0) {
        // --- CASE A: Linear Miter (Outer Corner to Inner Corner) ---
        // Normalize the pixel's depth within the side thickness
        float2 distFromEdge = outerSize - abs(p);
        float2 norm = distFromEdge / max(float2(0.0001), t);

        // The side with the "shallower" normalized distance wins
        if (norm.x < norm.y) {
            return (s.x > 0.0) ? rightColor : leftColor;
        } else {
            return (s.y > 0.0) ? bottomColor : topColor;
        }
    } else {
        // --- CASE B: Aspect Ratio Split ---
        // When inner hole is still rounded, we split based on the
        // relative position of the pixel within the quad.
        if (abs(p.x / outerSize.x) > abs(p.y / outerSize.y)) {
            return (s.x > 0.0) ? rightColor : leftColor;
        } else {
            return (s.y > 0.0) ? bottomColor : topColor;
        }
    }
}

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID){
    UniformBuffer gui = uniformBuffer[instanceID];

    VSOutput output;
    output.pos                  = mul(sb.viewProjection, mul(gui.model, float4(input.inPos, 1.0)));

    //Texture
    output.imageUV              = input.inTexCoord * gui.imageOffsetAndTiling.zw + gui.imageOffsetAndTiling.xy;
    output.imageID              = gui.imageID;
    output.imageRepeatMode      = gui.imageRepeatMode;

    //Colors
    output.bgColor              = gui.backgroundColor;
    output.imageColor           = gui.imageColor;
    output.borderBottomColor    = gui.borderBottomColor;
    output.borderTopColor       = gui.borderTopColor;
    output.borderLeftColor      = gui.borderLeftColor;
    output.borderRightColor     = gui.borderRightColor;

    //Outer Shape
    float2 rectSize             = float2(gui.model[0][0], gui.model[1][1]);
    float2 bgTiling             = rectSize * 0.5;
    float2 bgOffset             = input.inTexCoord * rectSize;
    float2 bgUV                 = bgOffset - bgTiling;
    output.outerRadius          = gui.cornerRadius;
    output.outerSize            = bgTiling - float2(gui.margin.y + gui.margin.w, gui.margin.x + gui.margin.z) * 0.5;
    output.outerP               = bgUV + float2(gui.margin.y - gui.margin.w,gui.margin.x - gui.margin.z) * 0.5;

    //Inner Shape
    float2 innerOffset = float2(gui.borderThickness.y + gui.borderThickness.w, gui.borderThickness.z + gui.borderThickness.x) * 0.5;
    float2 innerCenter = float2(gui.borderThickness.y - gui.borderThickness.w, gui.borderThickness.z - gui.borderThickness.x) * 0.5;
    output.innerRadius = adjustedInnerRadius(output.outerSize, bgOffset, bgTiling, gui.borderThickness, gui.cornerRadius);
    output.innerSize = output.outerSize - innerOffset;
    output.innerP = output.outerP + innerCenter;

    //Border
    output.borderThickness = gui.borderThickness;

    return output;
}

// Correct declaration for array of combined image samplers
[[vk::binding(2, 0)]] Texture2D textureArray[];
[[vk::binding(3, 0)]] SamplerState guiSampler[];
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET{
    float1 outerDist = sdRoundedBox(vertIn.outerP, vertIn.outerSize, vertIn.outerRadius);
    float1 outerAA = fwidth(outerDist);
    float1 outerMask = 1.0 - smoothstep(-outerAA, outerAA, outerDist);

    float1 innerDist = sdRoundedBox(vertIn.innerP,vertIn.innerSize,vertIn.innerRadius);
    float1 innerAA = fwidth(innerDist);
    float1 innerMask = smoothstep(-innerAA, innerAA, innerDist);

    float4 borderColor = selectBorderColor(vertIn.outerP, vertIn.outerSize,vertIn.innerP, vertIn.innerSize, vertIn.innerRadius,
            vertIn.borderTopColor, vertIn.borderBottomColor, vertIn.borderLeftColor, vertIn.borderRightColor);


    float4 color = lerp(vertIn.bgColor, borderColor,float4(innerMask));
    float4 finalColor = float4(color.rgb, color.a * outerMask);

    return finalColor;
}