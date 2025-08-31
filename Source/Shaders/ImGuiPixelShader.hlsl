Texture2D tex0 : register(t0);
SamplerState samLinear : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PS_IN input) : SV_TARGET
{
    float4 c = input.col * tex0.Sample(samLinear, input.uv);
    return c;
}

