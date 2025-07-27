cbuffer TextInputData : register(b0)
{
    float2 size;
    float2 objectPos;
    float2 screenSize;
}

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(float3 pos : POSITION)
{
    VS_OUTPUT output;
    pos.x *= size.x; // scale vertex to scale the whole triangle
    pos.y *= size.y;

    pos.x += objectPos.x / screenSize.x;
    
    pos.x = (pos.x * 2.0f) - 1.0f; // convert from normal 0,1 to weird -1 ,1
    pos.y += 1.0f - (objectPos.y / screenSize.y);
		
    output.position = float4(pos, 1.0f);
	
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f); // Black to Blue, fully opaque
	
    return output;
};