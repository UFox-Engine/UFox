struct VSInput {
    float3 inPos;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer {
    uint32_t imageID;
    uint32_t imageRepeatMode;
    float4x4 model;
    float4   imageColor;
    float4   bgColor;
    float4   uvRect;
};

struct ScreenBuffer {
    float4x4 viewProjection;
};

[[vk::binding(0, 0)]] StructuredBuffer<UniformBuffer> uniformBuffer;
[[vk::binding(1, 0)]] ConstantBuffer<ScreenBuffer>    sb;

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float4 bgColor;
    uint   imageID;
    uint   imageRepeatMode;
    float4 imageColor;      // passed from vertex
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID)
{
    UniformBuffer gui = uniformBuffer[instanceID];

    VSOutput output;
    output.pos          = mul(sb.viewProjection, mul(gui.model, float4(input.inPos, 1.0)));
    output.fragColor    = input.inColor;
    output.fragTexCoord = input.inTexCoord * gui.uvRect.zw + gui.uvRect.xy;;
    output.bgColor      = gui.bgColor;
    output.imageID      = gui.imageID;
    output.imageRepeatMode = gui.imageRepeatMode;
    output.imageColor   = gui.imageColor;

    return output;
}

// Correct declaration for array of combined image samplers
Texture2D textureArray[];
SamplerState guiSampler[];
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    // Sample the texture
        float4 texColor = textureArray[vertIn.imageID].Sample(guiSampler[vertIn.imageRepeatMode],vertIn.fragTexCoord);
        float4 tintedTexColor = texColor * vertIn.imageColor;
        float4 finalColor = vertIn.bgColor * (1.0 - tintedTexColor.a) + tintedTexColor * tintedTexColor.a;

        return finalColor;
}