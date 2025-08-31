struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

Texture2D g_fontTexture : register(t0);
SamplerState g_samplerState : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float4 sample = g_fontTexture.Sample(g_samplerState, input.uv);
    // Some atlases pack coverage in R; prefer max(R, A) for robustness
    float a = max(sample.r, sample.a);
    return float4(1.0, 1.0, 1.0, a);
}
