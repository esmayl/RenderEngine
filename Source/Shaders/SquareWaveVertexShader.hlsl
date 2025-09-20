cbuffer VertexInputData : register(b0)
{
    float2 size;
    float2 objectPos;
    float aspectRatio;
    float time;
    int2 indexes;
    float speed;
    int2 grid;
    float padding1;
    float2 targetPos;
    float orbitDistance;
    float jitter;
    float2 previousTargetPos;
    float flockTransitionTime;
    float deltaTime;
    int activeFoodIndex;
    float cameraPosX;
    float cameraPosY;
    float cameraZoom;
    float hazardPosX;
    float hazardPosY;
    float hazardRadius;
    int hazardActive;
}

struct VsInput
{
    float3 pos : POSITION;
    uint instanceId : SV_InstanceID;
    float2 instancePos : INSTANCEPOS; // This receives the per-instance data
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION; // Mandatory: Vertex position for rasterization
    float4 color : COLOR;
    
};

VS_OUTPUT main(VsInput input)
{
    VS_OUTPUT output;
    
    uint i = input.instanceId % grid.x; // go from 0 to grid.x
    uint j = input.instanceId / grid.x; // increase by 1 every time we go past the grid.x
    
    input.pos.x /= aspectRatio;
    
    input.pos.x *= size.x * 0.5f; // scale vertex to scale the whole triangle
    input.pos.y *= size.y * 0.55f;
    input.pos.x += (float(i) / grid.x * 2.0f) - 1.0f;
    input.pos.y += 1.0f - (float(j) / grid.y * 2.0f);
	
    //pos.x += (objectPos.x * 2.0f) - 1.0f; // convert from normal 0,1 to weird -1 ,1
	//pos.y += 1.0f - (objectPos.y * 2.0f);
	//pos.y += sin(time * speed - (indexes.x + indexes.y)) *0.03f; // waves!
		
    float sinWave = sin(time * speed - ((float)i + j)) * 0.03f;
    input.pos.y += sinWave;
	
    float2 cameraOffset = float2(cameraPosX, cameraPosY);
    input.pos.xy -= cameraOffset;

    output.position = float4(input.pos, 1.0f);
	
    // Subtle teal tint with mild shimmering based on phase
    float phase = sin(time * speed - ((float)i + j));
    float tint = 0.7f + 0.3f * (0.5f + 0.5f * phase); // 0.7..1.0
    output.color = float4(0.0f, 0.60f * tint, 0.55f * tint, 0.2f);
	
	return output;
};
