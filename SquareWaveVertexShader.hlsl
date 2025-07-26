cbuffer VertexInputData : register(b0)
{
	float2 size;
	float2 objectPos; // objectPosX and objectPosY from C++ map here
	float aspectRatio;
	float time;
	float2 indexes;
	float speed;
}

struct VS_OUTPUT
{
    float4 position : SV_POSITION; // Mandatory: Vertex position for rasterization
    float4 color : COLOR;
};

VS_OUTPUT main(float3 pos : POSITION)
{
    VS_OUTPUT output;
	pos.x *= size.x; // scale vertex to scale the whole triangle
	pos.y *= size.y;
	pos.x += (objectPos.x * 2.0f) - 1.0f; // convert from normal 0,1 to weird -1 ,1
	pos.y += 1.0f - (objectPos.y * 2.0f);
	pos.y += sin(time * speed - (indexes.x + indexes.y)) *0.03f; // waves!
		
    output.position = float4(pos, 1.0f);
	
    float b = saturate(1 - objectPos.y);
    output.color = float4(0.0f, 0.0f, b,1.0f); // Black to Blue, fully opaque
	
	return output;
};