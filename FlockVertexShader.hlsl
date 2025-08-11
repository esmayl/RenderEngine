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
    float orbitDistance;
    float jitter;
    float2 previousTargetPos;
    float flockTransitionTime;
    float flockFrozenTime;
}
StructuredBuffer<float2> CurrPos : register(t0); // same as CurrPosIn after swap

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
    
    float2 quad = float2(input.pos.x / aspectRatio, input.pos.y);
    float2 finalPos = quad + CurrPos[input.instanceId];
    
    output.position = float4(finalPos, 0.0f, 1.0f);
    output.color = float4(1, 1, 1, 1);
	return output;
}