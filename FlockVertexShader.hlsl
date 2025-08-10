cbuffer VertexInputData : register(b0)
{
    float2 size;
    float2 objectPos; // objectPosX and objectPosY from C++ map here
    float aspectRatio;
    float time;
    int2 indexes;
    float speed;
    int2 grid;
    float2 targetPos;
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
    
    float distanceToPos = distance(input.instancePos, targetPos);
    distanceToPos = 1 / (distanceToPos + 0.001f);
    
    float t = saturate(time * speed * distanceToPos);
    float2 movedInst = lerp(input.instancePos, targetPos, t);
    
    float2 finalPos = input.pos.xy + movedInst;
    
    output.position = float4(finalPos, 0.0f, 1.0f);
    output.color = float4(targetPos.x, targetPos.y, 0.0f, 1.0f);
	return output;
}