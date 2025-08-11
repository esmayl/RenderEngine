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

StructuredBuffer<float2> CurrPosIn : register(t0);
RWStructuredBuffer<float2> CurrPosOut : register(u0);

[numthreads(256, 1, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{   
    float2 pos = CurrPosIn[threadId.x];
    
    float distanceToPos = distance(pos, targetPos);
    
    // Calculate the speed for each vertex, move faster if the vertex is closer to the targetPos
    float speedFactor = 1.0 / (distanceToPos + 0.001f);
    float t = saturate(flockTransitionTime * speed * speedFactor);
    float2 movedInst = lerp(pos, targetPos, t);
    float4 color = float4(0, 0, 0, 1);
    
    float2 differenceInIsoSpace = float2((movedInst.x - targetPos.x) * aspectRatio, (movedInst.y - targetPos.y));
    
    if (length(differenceInIsoSpace) > orbitDistance)
    {
        color.r = distanceToPos;
        pos = movedInst;
    }
    else
    {
        
        float angle = speed * flockTransitionTime + (float) threadId.x; // radians, simple spin
        color.r = angle;
        float2 orbit = float2(cos(angle), sin(angle));
        orbit.x /= aspectRatio;
        pos = targetPos + orbit * (orbitDistance * (1.0f + jitter * (threadId.x * 2.0f - 1.0)));
    }
    
    CurrPosOut[threadId.x] = pos;
}