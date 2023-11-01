
Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct VSInput
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD;
    float4 color    : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color    : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    input.position /= 100;
    input.position.z = 0;
    input.position.w = 1;
    result.position = input.position;
    result.texCoord = input.texCoord;
    result.color = input.color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return t1.Sample(s1, input.texCoord);
}