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
    float4          imageOffsetAndTiling;
    SDRectInputData sdInput;
    float2          scrollingOffset;
    uint            styleIndex;
    uint            imageIndex;
    uint            tailPadding[2];
};

struct StyleStaticData {
    float4 radius;
    float4 iRadius;
    float4 imageColor;
    float4 backgroundColor;
    float4 borderBottomColor;
    float4 borderTopColor;
    float4 borderLeftColor;
    float4 borderRightColor;

    uint  imageRepeatMode;
    uint  isFont;
    uint  tailPadding[2];
};

struct ScreenData {
    float4x4 viewProjection;
};

[[vk::binding(0, 0)]] StructuredBuffer<ElementData>   elementBuffer;
[[vk::binding(1, 0)]] StructuredBuffer<StyleStaticData> styleBuffer;
[[vk::binding(2, 0)]] ConstantBuffer<ScreenData>      sb;

[[vk::binding(3, 0)]] Texture2D    textureArray[];
[[vk::binding(4, 0)]] SamplerState guiSampler[];
//[[vk::binding(6,0)]] StructuredBuffer<GlyphGpuData> glyphBuffer[];

struct VSOutput {
    float4 pos : SV_Position;

    float2 imageUV;
    uint imageID;
    uint imageRepeatMode;
    uint isFont;

    float fontSize;

    float4 bgColor;
    float4 imageColor;
    float4 borderBottomColor;
    float4 borderTopColor;
    float4 borderLeftColor;
    float4 borderRightColor;

    float4 outerRadius;
    float2 outerSize;
    float2 outerP;

    float4 innerRadius;
    float2 innerSize;
    float2 innerP;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID, uint baseInstanceOffset : SV_StartInstanceLocation){
    uint instanceIndex = instanceID + baseInstanceOffset;

    ElementData el = elementBuffer[instanceIndex];
    StyleStaticData style = styleBuffer[el.styleIndex];
    SDRectInputData sd = el.sdInput;

    VSOutput o;
    o.pos = mul(sb.viewProjection, mul(el.model, float4(input.inPos, 1.0)));

    // Colors
    o.bgColor           = style.backgroundColor;
    o.imageColor        = style.imageColor;
    o.borderBottomColor = style.borderBottomColor;
    o.borderTopColor    = style.borderTopColor;
    o.borderLeftColor   = style.borderLeftColor;
    o.borderRightColor  = style.borderRightColor;

    // Shape Data
    o.outerRadius = style.radius;
    o.outerSize   = sd.size;
    o.outerP      = input.inTexCoord * sd.offsetAndTiling.zw - sd.offsetAndTiling.xy + sd.pOffset;

    o.innerRadius = style.iRadius;
    o.innerSize   = sd.iSize;
    o.innerP      = o.outerP + sd.iPOffset;

    // Texture UV
    o.imageUV         = (input.inTexCoord + el.imageOffsetAndTiling.xy) * el.imageOffsetAndTiling.zw;
    o.imageID         = el.imageIndex;
    o.imageRepeatMode = style.imageRepeatMode;
    o.isFont          = style.isFont;

    return o;
}

// Rounded Box SDF
float sdRoundedBox(float2 p, float2 b, float4 r){
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

static const float PX_RANGE = 8.0;

// Median filter
float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float4 sampleGlyph(VSOutput i)
{
    Texture2D tex = textureArray[i.imageID];   // fix #6

    float2 texSize;
    tex.GetDimensions(texSize.x, texSize.y);

    float4 s  = tex.Sample(guiSampler[0], i.imageUV);
    float  sd = median3(s.r, s.g, s.b);

    // Screen-space pixel range (the standard msdf-atlas-gen formula)
    float2 unitRange      = PX_RANGE / texSize;
    float2 screenTexSize  = 1.0 / max(fwidth(i.imageUV), 1e-6);
    float  screenPxRange  = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    float opacity = clamp(screenPxRange * (sd - 0.5) + 0.5, 0.0, 1.0);

    // s.a holds the true SDF (MTSDF). Use it for outlines/glow later:
    // float outline = clamp(screenPxRange * (s.a - 0.5 + outlineWidth) + 0.5, 0, 1);

    return float4(i.imageColor.rgb, opacity * i.imageColor.a);
}



// Your original smart border color selector
float4 selectBorderColor(float2 outerP, float2 outerSize, float2 innerP, float2 innerSize,
                         float4 topColor, float4 bottomColor, float4 leftColor, float4 rightColor){
    float2 p = outerP;
    float2 s = sign(p);

    float2 t;
    t.x = (s.x > 0.0) ? (outerSize.x - innerSize.x - (outerP.x - innerP.x))
                      : (outerSize.x - innerSize.x + (outerP.x - innerP.x));
    t.y = (s.y > 0.0) ? (outerSize.y - innerSize.y - (outerP.y - innerP.y))
                      : (outerSize.y - innerSize.y + (outerP.y - innerP.y));

    float2 distFromEdge = outerSize - abs(p);
    float2 norm = distFromEdge / max(float2(0.0001), t);

    if (norm.x < norm.y) {
         return (s.x > 0.0) ? rightColor : leftColor;
    } else {
         return (s.y > 0.0) ? bottomColor : topColor;
    }
}

[shader("fragment")]
float4 fragMain(VSOutput i) : SV_TARGET
{
    float outerDist = sdRoundedBox(i.outerP, i.outerSize, i.outerRadius);
    float outerMask = 1.0 - smoothstep(0.0, 2.0, outerDist);

    float innerDist = sdRoundedBox(i.innerP, i.innerSize, i.innerRadius);
    float innerMask = smoothstep(0.0, 3.0, innerDist);

    float4 borderColor = selectBorderColor(i.outerP, i.outerSize, i.innerP, i.innerSize,i.borderTopColor, i.borderBottomColor, i.borderLeftColor, i.borderRightColor);

    float4 texColor = (i.isFont != 0)? sampleGlyph(i) : textureArray[i.imageID].Sample(guiSampler[i.imageRepeatMode], i.imageUV) * i.imageColor;

    // fix #2: proper "over" compositing (non-premultiplied output for SrcAlpha blend)
    float  aOut  = texColor.a + i.bgColor.a * (1.0 - texColor.a);
    float3 cOut  = (texColor.rgb * texColor.a + i.bgColor.rgb * i.bgColor.a * (1.0 - texColor.a))/ max(aOut, 1e-5);

    // border over the (bg+image) result
    float  bA    = borderColor.a * innerMask;
    float  fA    = bA + aOut * (1.0 - bA);
    float3 fC    = (borderColor.rgb * bA + cOut * aOut * (1.0 - bA)) / max(fA, 1e-5);

    return float4(fC, fA * outerMask);
}