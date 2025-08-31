cbuffer VSConstants : register(b0)
{
    float4x4 ProjectionMatrix;
}

struct VS_IN
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

VS_OUT main(VS_IN input)
{
    VS_OUT o;
    o.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.0f, 1.0f));
    o.uv = input.uv;
    o.col = input.col;
    return o;
}

