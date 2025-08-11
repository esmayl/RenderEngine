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
    float2 finalPos;
    
    float2 quad = float2(input.pos.x / aspectRatio, input.pos.y);
    
    float2 startPos = lerp(input.instancePos, previousTargetPos, flockFrozenTime);
    
    float distanceToPos = distance(startPos, targetPos);
    // Calculate the speed for each vertex, move faster if the vertex is closer to the targetPos
    float speedFactor = 1.0 / (distanceToPos + 0.001f);
    float t = saturate(flockTransitionTime * speed * speedFactor);
    float2 movedInst = lerp(startPos, targetPos, t);
    float4 color = float4(0, 0, 0, 1);
    
    float2 differenceInIsoSpace = float2((movedInst.x - targetPos.x) * aspectRatio, (movedInst.y - targetPos.y));
    
    if (length(differenceInIsoSpace) > orbitDistance)
    {
        color.r = distanceToPos;
        finalPos = quad + movedInst;
    }
    else
    {
        
        float angle = speed * flockTransitionTime + (float) input.instanceId; // radians, simple spin
        color.r = angle;
        float2 orbit = float2(cos(angle), sin(angle));
        orbit.x /= aspectRatio;
        finalPos = quad + targetPos + orbit * (orbitDistance * (1.0f + jitter * (input.instanceId * 2.0f - 1.0)));
    }
    
    output.position = float4(finalPos, 0.0f, 1.0f);
    output.color = color;
	return output;
}