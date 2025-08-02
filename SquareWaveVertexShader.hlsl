cbuffer VertexInputData : register(b0)
{
	float2 size;
	float2 objectPos; // objectPosX and objectPosY from C++ map here
	float aspectRatio;
	float time;
	int2 indexes;
	float speed;
    int2 grid;
}

struct VS_OUTPUT
{
    float4 position : SV_POSITION; // Mandatory: Vertex position for rasterization
    float4 color : COLOR;
};

VS_OUTPUT main(float3 pos : POSITION, uint instanceId : SV_InstanceID)
{
    VS_OUTPUT output;
    
    uint i = instanceId % grid.x; // go from 0 to grid.x
    uint j = instanceId / grid.x; // increase by 1 every time we go past the grid.x
	
    pos.x *= size.x; // scale vertex to scale the whole triangle
    pos.y *= size.y * 0.6f;
    
    pos.x += (float(i) / grid.x * 2.0f) - 1.0f;
    pos.y += 1.0f - (float(j) / grid.y * 2.0f);
	
    //pos.x += (objectPos.x * 2.0f) - 1.0f; // convert from normal 0,1 to weird -1 ,1
	//pos.y += 1.0f - (objectPos.y * 2.0f);
	//pos.y += sin(time * speed - (indexes.x + indexes.y)) *0.03f; // waves!
		
    float sinWave = sin(time * speed - ((float)i + j)) * 0.03f;
    pos.y += sinWave;
	
    output.position = float4(pos, 1.0f);
	
    float b = saturate(1 - sinWave);
    output.color = float4(0.0f, 0.0f, b,1.0f); // Black to Blue, fully opaque
	
	return output;
};