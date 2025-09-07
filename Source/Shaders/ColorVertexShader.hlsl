cbuffer VertexInputData : register(b0)
{
    float2 size;          // sizeX, sizeY
    float2 objectPos;     // objectPosX, objectPosY (NDC)
    float aspectRatio;
    float time;
    int2 indexes;         // indexesX used as color code
    float speed;
    int2 grid;
    float2 targetPos;
    float orbitDistance;
    float jitter;
    float2 previousTargetPos;
    float flockTransitionTime;
    float deltaTime;
    int activeFoodIndex;
    int3 _paddingC;
    float2 hazardPos;
    float hazardRadius;
    int hazardActive;
}

struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float2 p = input.pos.xy;
    p.x /= aspectRatio;

    // Scale relative to the base quad size (SquareMesh uses ~0.05 unit size)
    p.x *= size.x;
    p.y *= size.y;

    // Offset to NDC object position
    p += objectPos;

    output.position = float4(p, 0.0f, 1.0f);

    // Choose color by indexes.x code
    if (indexes.x == 1)
    {
        output.color = float4(0.2f, 0.9f, 0.2f, 1.0f); // food - green
    }
    else if (indexes.x == 2)
    {
        output.color = float4(0.95f, 0.2f, 0.2f, 1.0f); // nest - red
    }
    else if (indexes.x == 3)
    {
        output.color = float4(1.0f, 0.9f, 0.2f, 1.0f); // active food - yellow
    }
    else if (indexes.x == 4)
    {
        output.color = float4(0.75f, 0.4f, 0.95f, 0.85f); // hazard - purple
    }
    else if (indexes.x == 5)
    {
        output.color = float4(0.08f, 0.10f, 0.12f, 0.6f); // UI panel - translucent dark
    }
    else if (indexes.x == 6)
    {
        output.color = float4(1.0f, 0.8f, 0.2f, 1.0f); // bonus sugar - gold
    }
    else
    {
        output.color = float4(1.0f, 1.0f, 1.0f, 1.0f); // default white
    }

    // Lighten effect for outlines: indexes.y = 1..3 increases lightening
    if (indexes.y > 0)
    {
        float t = saturate(0.15f * indexes.y);
        float a = output.color.a * (0.7f + 0.1f * indexes.y);
        output.color.rgb = lerp(output.color.rgb, float3(1.0f, 1.0f, 1.0f), t);
        output.color.a = a;
    }

    return output;
}
