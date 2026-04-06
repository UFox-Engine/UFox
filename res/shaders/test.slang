struct VSInput {
    float3 inPos;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer {
    uint32_t imageID;
    float4x4 model;
    float4   imageColor;
    float4   bgColor;
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
    float4 imageColor;      // passed from vertex
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID)
{
    UniformBuffer gui = uniformBuffer[instanceID];

    VSOutput output;
    output.pos          = mul(sb.viewProjection, mul(gui.model, float4(input.inPos, 1.0)));
    output.fragColor    = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.bgColor      = gui.bgColor;
    output.imageID      = gui.imageID;
    output.imageColor   = gui.imageColor;

    return output;
}

// Correct declaration for array of combined image samplers
Sampler2D textureArray[];

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    // Sample the texture
        float4 texColor = textureArray[vertIn.imageID].Sample(vertIn.fragTexCoord);
        float4 tintedTexColor = texColor * vertIn.imageColor;
        float4 finalColor = vertIn.bgColor * (1.0 - tintedTexColor.a) + tintedTexColor * tintedTexColor.a;

        return finalColor;
}