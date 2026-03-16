struct VSInput {
    float3 inPos;
    float3 inColor;
    float2 inTexCoord;
};

struct RectData{
    float4x4 model;
};

struct ScreenBuffer {
    float4x4 viewProjection;
};

StructuredBuffer<RectData> rectBuffer;
ConstantBuffer<ScreenBuffer> sb;

struct VSOutput{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
        RectData rect = rectBuffer[instanceID];
        output.pos = mul(sb.viewProjection, mul(rect.model, float4(input.inPos, 1.0)));
        output.fragColor = input.inColor;
        output.fragTexCoord = input.inTexCoord;
        return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}