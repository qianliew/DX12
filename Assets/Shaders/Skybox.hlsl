
cbuffer GlobalConstants : register(b0)
{
    float4x4 WorldToProjectionMatrix;
    float3 CameraPositionWS;
};

cbuffer PerObjectConstants : register(b1)
{
    float4x4 ObjectToWorldMatrix;
};

TextureCube SkyboxCube : register(t1);
SamplerState SkyboxSampler : register(s1);

struct VSInput
{
    float4 positionOS   : POSITION;
    float2 texCoord     : TEXCOORD;
};

struct PSInput
{
    float4 positionCS   : SV_POSITION;
    float3 texCoord     : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    input.positionOS.w = 1;
    result.positionCS = mul(WorldToProjectionMatrix, mul(ObjectToWorldMatrix, input.positionOS));
    result.texCoord = input.positionOS.xyz;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return SkyboxCube.Sample(SkyboxSampler, input.texCoord);
}
