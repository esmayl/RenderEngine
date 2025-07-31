cbuffer TextInputData : register(b0)
{
    float2 size;
    float2 objectPos;
    float2 screenSize;
    float2 uvOffset;
    float2 uvScale;
}

struct VS_INPUT
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

Texture2D g_fontTexture : register(t0);
SamplerState g_samplerState : register(s0);


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float vertexDistance = 0.05f;
    
    float aspect = screenSize.x / screenSize.y;
    float pixelHeight = 2.0f / screenSize.y;
    float pixelWidth = 2.0f / screenSize.x;
    
    input.position.x = (input.position.x / vertexDistance) * size.x * pixelWidth; // Convert including aspect ratio
    input.position.y = (input.position.y / vertexDistance) * size.y * pixelHeight;
    input.position.x += objectPos.x * pixelWidth - 1.0f; // convert from normal 0,1 to weird -1,1 - also takes into a account the aspect ratio now
    input.position.y += 1.0f - objectPos.y * pixelHeight;
		
    output.position = input.position;
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    output.uv = (input.uv * uvScale) + uvOffset;
    return output;
};