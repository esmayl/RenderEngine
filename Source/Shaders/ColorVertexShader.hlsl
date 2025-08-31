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
    else
    {
        output.color = float4(1.0f, 1.0f, 1.0f, 1.0f); // default white
    }

    return output;
}
