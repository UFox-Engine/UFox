struct VSInput {
    float3 inPos;
    float3 inNormal;
    float2 inTexCoord;
    float3 inColor;
};

struct SDRectInputData {
    float4 offsetAndTiling;
    float2 size;
    float2 pOffset;
    float2 iSize;
    float2 iPOffset;
};

struct ElementData {
    float4x4        model;
    SDRectInputData sdInput;
    float2          scrollingOffset;
    uint32_t        styleIndex;
    uint32_t        padding;
};

struct StyleStaticData {
    float4 radius;
    float4 iRadius;
    float4 imageOffsetAndTiling;
    float4 imageColor;
    float4 backgroundColor;
    float4 borderBottomColor;
    float4 borderTopColor;
    float4 borderLeftColor;
    float4 borderRightColor;

    uint32_t  imageID;
    uint32_t  imageRepeatMode;
    uint32_t  tailPadding[2];
};

struct UniformData {
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

struct ShapeData {
    float4      radius;
    float2      size;
    float2      p;
};

struct ScreenData {
    float4x4 viewProjection;
};

[[vk::binding(0, 0)]] StructuredBuffer<UniformData> uniformBuffer;
[[vk::binding(1, 0)]] ConstantBuffer<ScreenData>    sb;

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
    ShapeData   outerShape;
    //Inner Shape
    ShapeData   innerShape;

    bool        isOverFlow;
};

const static float4 black = float4(0.0, 0.0, 0.0, 1.0);
const static float4 white = float4(1.0, 1.0, 1.0, 1.0);

float1 sdRoundedBox(ShapeData data) {
    float radius;
        if (data.p.x > 0.0) {
            radius = (data.p.y > 0.0) ? data.radius.x : data.radius.y;
        } else {
            radius = (data.p.y > 0.0) ? data.radius.z : data.radius.w;
        }

    float2 q = abs(data.p) - data.size + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float4 adjustedInnerRadius(float4 b, float4 r) {
    return max(float4(0.0), r - float4(
        max(b.z, b.y),     // BR
        max(b.x, b.y),     // TR
        max(b.z, b.w),     // BL
        max(b.x, b.w)));   // TL
}


float4 selectBorderColor( float2 outerP, float2 outerSize, float2 innerP, float2 innerSize, float4 topColor, float4 bottomColor, float4 leftColor, float4 rightColor) {
    float2 p = outerP;
    float2 s = sign(p);

    float2 t;
    t.x = (s.x > 0.0) ? (outerSize.x - innerSize.x - (outerP.x - innerP.x))  // Right
                      : (outerSize.x - innerSize.x + (outerP.x - innerP.x)); // Left
    t.y = (s.y > 0.0) ? (outerSize.y - innerSize.y - (outerP.y - innerP.y))  // Top
                      : (outerSize.y - innerSize.y + (outerP.y - innerP.y)); // Bottom

    float2 distFromEdge = outerSize - abs(p);
    float2 norm = distFromEdge / max(float2(0.0001), t);

    if (norm.x < norm.y) {
         return (s.x > 0.0) ? rightColor : leftColor;
    } else {
         return (s.y > 0.0) ? bottomColor : topColor;
    }
}


[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID, uint baseInstanceOffset : SV_StartInstanceLocation){

    uint instanceIndex = instanceID + baseInstanceOffset;
    UniformData gui = uniformBuffer[instanceIndex];

    VSOutput output;
    output.pos                  = mul(sb.viewProjection, mul(gui.model, float4(input.inPos, 1.0)));

    //Colors
    output.bgColor              = gui.backgroundColor;
    output.imageColor           = gui.imageColor;
    output.borderBottomColor    = gui.borderBottomColor;
    output.borderTopColor       = gui.borderTopColor;
    output.borderLeftColor      = gui.borderLeftColor;
    output.borderRightColor     = gui.borderRightColor;

    //Outer Shape
    float2 bgTiling             = float2(gui.model[0][0], gui.model[1][1]);
    float2 bgOffset             = bgTiling * 0.5;
    float2 bgUV                 = input.inTexCoord * bgTiling - bgOffset;
    float2 marginSpace          = float2(gui.margin.y + gui.margin.w, gui.margin.z + gui.margin.x);
    output.outerShape.radius    = gui.cornerRadius;
    output.outerShape.size      = bgOffset - marginSpace * 0.5;
    output.outerShape.p         = bgUV + float2(gui.margin.y - gui.margin.w,gui.margin.z - gui.margin.x) * 0.5 ;


    //Inner Shape
    float2 borderSpace          = float2( gui.borderThickness.y + gui.borderThickness.w, gui.borderThickness.z + gui.borderThickness.x);
    float2 innerOffset          = borderSpace * 0.5;
    float2 innerCenter          = float2(gui.borderThickness.y - gui.borderThickness.w, gui.borderThickness.z - gui.borderThickness.x) * 0.5;
    output.innerShape.radius    = adjustedInnerRadius(gui.borderThickness, gui.cornerRadius);
    output.innerShape.size      = output.outerShape.size - innerOffset;
    output.innerShape.p         = output.outerShape.p + innerCenter;


    //Texture
    output.imageUV              = (input.inTexCoord + gui.imageOffsetAndTiling.xy)* gui.imageOffsetAndTiling.zw;
    output.imageID              = gui.imageID;
    output.imageRepeatMode      = gui.imageRepeatMode;

    return output;
}

// Correct declaration for array of combined image samplers
[[vk::binding(2, 0)]] Texture2D textureArray[];
[[vk::binding(3, 0)]] SamplerState guiSampler[];
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET{
    float1 outerDist = sdRoundedBox(vertIn.outerShape) ;
    float1 outerAA = fwidth(outerDist);
    float1 outerMask = 1.0 - smoothstep(-outerAA, outerAA, outerDist);

    if (outerMask <= 0.17) {
        discard;
    }

    float1 innerDist = sdRoundedBox(vertIn.innerShape);
    float1 innerAA = fwidth(innerDist);
    float1 innerMask = smoothstep(-innerAA, innerAA, innerDist);

    float4 borderColor = selectBorderColor(vertIn.outerShape.p, vertIn.outerShape.size,vertIn.innerShape.p, vertIn.innerShape.size,
            vertIn.borderTopColor, vertIn.borderBottomColor, vertIn.borderLeftColor, vertIn.borderRightColor);

    float4 texColor = textureArray[vertIn.imageID].Sample(guiSampler[vertIn.imageRepeatMode],vertIn.imageUV);
    float4 tintedTexColor = vertIn.imageColor * texColor;
    float4 finalBGColor = vertIn.bgColor * (1.0 - tintedTexColor.a) + tintedTexColor * tintedTexColor.a;

    float4 color = lerp(finalBGColor, borderColor,float4(innerMask));
    float4 finalColor = float4(color.rgb, color.a * outerMask);


    return finalColor;
}