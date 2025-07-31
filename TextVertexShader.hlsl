cbuffer TextInputData : register(b0)
{
    float2 size;
    float2 objectPos;
    float2 screenSize;
    float2 uvOffset;
    float2 uvScale;
}

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT main(float3 pos : POSITION)
{
    VS_OUTPUT output;
    
    float vertexDistance = 0.05f;
    
    float aspect = screenSize.x / screenSize.y;
    float pixelHeight = 2.0f / screenSize.y;
    float pixelWidth = 2.0f / screenSize.x;
    
    pos.x = (pos.x / vertexDistance) * size.x * pixelWidth; // Convert including aspect ratio
    pos.y = (pos.y / vertexDistance) * size.y * pixelHeight;

    pos.x += objectPos.x * pixelWidth - 1.0f; // convert from normal 0,1 to weird -1,1 - also takes into a account the aspect ratio now
    pos.y += 1.0f - objectPos.y * pixelHeight;
		
    output.position = float4(pos, 1.0f);
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f); // Black to Blue, fully opaque

    return output;
};