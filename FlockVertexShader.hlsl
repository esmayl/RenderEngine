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
    float2 targetPos = float2(0, 0);
    float targetDistance = distance(input.instancePos, targetPos);
    
    float2 direction = -normalize(input.instancePos);
    
    float t = saturate(time / speed);
    
    float2 movedInst = lerp(input.instancePos, float2(0, 0), t);

    
    //float calculatedSpeed = speed * time;
    //calculatedSpeed = min(calculatedSpeed, targetDistance);
    
    //float2 movedInstancePos = input.instancePos * (1 - t);
    
    float2 finalPos = input.pos.xy - movedInst;
    
    output.position = float4(finalPos, 0.0f, 1.0f);
    output.color = float4(input.instanceId, 0.0f, 0.0f, 1.0f);
	return output;
}