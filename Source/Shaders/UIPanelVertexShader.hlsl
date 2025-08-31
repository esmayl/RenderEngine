cbuffer UIPanelInputData : register(b0)
{
    float2 size;        // pixel width, height
    float2 objectPos;   // pixel top-left
    float2 screenSize;  // pixel screen size
    float4 color;       // RGBA
}

struct VS_INPUT
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0; // unused
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float vertexDistance = 0.05f;

    float pixelHeight = 2.0f / screenSize.y;
    float pixelWidth  = 2.0f / screenSize.x;

    // Scale base quad to pixel size and position at top-left objectPos
    input.position.x = (input.position.x / vertexDistance) * size.x * pixelWidth;
    input.position.y = (input.position.y / vertexDistance) * size.y * pixelHeight;
    input.position.x += objectPos.x * pixelWidth - 1.0f;
    input.position.y += 1.0f - objectPos.y * pixelHeight;

    output.position = input.position;
    output.color = color;
    return output;
}

