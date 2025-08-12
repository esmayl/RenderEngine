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

// stable per-instance random in [0,1)
float Rand01(uint n)
{
    n ^= 2747636419u;
    n *= 2654435769u;
    n ^= n >> 16;
    n *= 2654435769u;
    n ^= n >> 16;
    return (n & 0x00FFFFFFu) / 16777215.0f;
}

[numthreads(256, 1, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{   
    float2 pos = CurrPosIn[threadId.x];
    
    float distanceToPos = distance(pos, targetPos);
    
    // Calculate the speed for each vertex, move faster if the vertex is closer to the targetPos
    float speedFactor = 1.0 / (distanceToPos + 0.001f);
    float t = saturate(flockTransitionTime * speed * speedFactor);
    float2 movedInst = lerp(pos, targetPos, t);
    
    float2 differenceInIsoSpace = float2((movedInst.x - targetPos.x) * aspectRatio, (movedInst.y - targetPos.y));
    float orbitDistancePlusJitter = (orbitDistance * (1.0f + jitter * (threadId.x * 2.0f - 1.0f)));
        
    float baseAngle = atan2(differenceInIsoSpace.y, differenceInIsoSpace.x);
    
    float pSeed = Rand01(threadId.x * 19349663u);
    float angle = baseAngle + speed * flockTransitionTime + (pSeed - 0.5f) * 0.3f; // radians, simple spin
    float2 orbit = float2(cos(angle), sin(angle)) * orbitDistancePlusJitter;
    orbit.x /= aspectRatio;
    
    float2 orbitPos = targetPos + orbit ;
    float alpha = saturate((orbitDistancePlusJitter - length(differenceInIsoSpace)) / (orbitDistance * 0.15f)); // 0.3f is feather distance
    
    pos = lerp(movedInst, orbitPos, alpha);
    
    CurrPosOut[threadId.x] = pos;
}