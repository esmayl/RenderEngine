cbuffer TextInputData : register(b0)
{
    float size;
    float2 objectPos;
    float aspectRatio;
    float2 uv;
}

struct VS_OUTPUT
{
    float4 position : SV_POSITION; // Mandatory: Vertex position for rasterization
    float4 color : COLOR;
};

VS_OUTPUT main(float3 pos : POSITION)
{
    VS_OUTPUT output;
    pos.x *= size * aspectRatio; // scale vertex to scale the whole triangle
    
    pos.x /= aspectRatio;
    pos.y *= size;
    
    pos.x += (objectPos.x * 2.0f) - 1.0f; // convert from normal 0,1 to weird -1 ,1
    pos.y += 1.0f - (objectPos.y * 2.0f);
		
    output.position = float4(pos, 1.0f);
    output.color = float4(0.0f, 0.0f, 0.0, 1.0f); // Black color? 
	
    return output;
};