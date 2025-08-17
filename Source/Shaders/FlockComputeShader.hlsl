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
    float deltaTime;
}

struct InstanceData
{
    float x;
    float y;
    float directionX;
    float directionY;
    float4 color;
    int movementState;
};

StructuredBuffer<InstanceData> CurrPosIn : register(t0);
RWStructuredBuffer<InstanceData> CurrPosOut : register(u0);

float noise01(uint id, uint tick)
{
    uint n = id * 374761393u + tick * 668265263u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    n ^= n >> 16u;
    return n * (1.0 / 4294967296.0);
}

[numthreads(256, 1, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{    
    float2 pos = float2(CurrPosIn[threadId.x].x, CurrPosIn[threadId.x].y);
    float stopDistance = 0.1f;
    float followDistance = 0.05f;
    float2 dirToGoal = normalize(targetPos - pos);

    CurrPosOut[threadId.x].color = CurrPosIn[threadId.x].color;
    
    if (threadId.x == 0)
    {
        float distToGoal = distance(pos, targetPos);

        if (distance(pos, targetPos) < stopDistance)
        {
            CurrPosOut[threadId.x].x = pos.x;
            CurrPosOut[threadId.x].y = pos.y;
            CurrPosOut[threadId.x].directionX = 0;
            CurrPosOut[threadId.x].directionY = 0;
        }
        else
        {
            dirToGoal = normalize(targetPos - pos);
            float step = min(distToGoal - stopDistance, speed * deltaTime);
            float2 newPos = pos + (dirToGoal * speed * deltaTime);
    
            CurrPosOut[threadId.x].x = newPos.x;
            CurrPosOut[threadId.x].y = newPos.y;
            CurrPosOut[threadId.x].directionX = dirToGoal;
            CurrPosOut[threadId.x].directionY = dirToGoal;
        }
               
        return;
    }
     
    float2 prevPos = float2(CurrPosIn[threadId.x - 1].x, CurrPosIn[threadId.x - 1].y);
    float2 toPreviousPos = prevPos - pos;
    float distanceToPreviousPos = length(toPreviousPos);
        
    if (distanceToPreviousPos <= followDistance)
    {
        CurrPosOut[threadId.x].x = pos.x;
        CurrPosOut[threadId.x].y = pos.y;
        CurrPosOut[threadId.x].directionX = 0;
        CurrPosOut[threadId.x].directionY = 0;
    }
    else
    {
        dirToGoal = toPreviousPos / distanceToPreviousPos;
        float step = min(distanceToPreviousPos - followDistance, speed * deltaTime);
            
        float2 newPos = pos + dirToGoal * step;
            
        CurrPosOut[threadId.x].x = newPos.x;
        CurrPosOut[threadId.x].y = newPos.y;
        CurrPosOut[threadId.x].directionX = dirToGoal;
        CurrPosOut[threadId.x].directionY = dirToGoal;
    }
}
    

//[numthreads(256, 1, 1)]
//void main(uint3 threadId : SV_DispatchThreadID)
//{
//    float2 pos = CurrPosIn[threadId.x];
    
//    float distanceToPos = distance(pos, targetPos);
    
//    float delay = saturate(distanceToPos / 2.0f); // 0.5f max delay time
//    delay += Rand01(threadId.x) * 0.5f;

//    // Local time for this instance
//    float localTime = max(flockTransitionTime - delay, 0.0f);
    
//    // Calculate the speed for each vertex, move faster if the vertex is closer to the targetPos
//    float speedFactor = 1.0 / (distanceToPos + 0.001f);
//    float t = saturate(localTime * speed * speedFactor);
//    float2 movedInst = lerp(pos, targetPos, t);
    
//    float2 differenceInIsoSpace = float2((movedInst.x - targetPos.x) * aspectRatio, (movedInst.y - targetPos.y));
//    float orbitDistancePlusJitter = (orbitDistance * (1.0f + jitter * (threadId.x * 2.0f - 1.0f)));
        
//    float baseAngle = atan2(differenceInIsoSpace.y, differenceInIsoSpace.x);
    
//    float pSeed = Rand01(threadId.x * 19349663u);
//    float angle = baseAngle + speed * flockTransitionTime + (pSeed - 0.5f) * 0.3f; // radians, simple spin
//    float2 orbit = float2(cos(angle), sin(angle)) * orbitDistancePlusJitter;
//    orbit.x /= aspectRatio;
    
//    float2 orbitPos = targetPos + orbit;
//    float alpha = saturate((orbitDistancePlusJitter - length(differenceInIsoSpace)) / (orbitDistance * 0.15f)); // 0.3f is feather distance
    
//    pos = lerp(movedInst, orbitPos, alpha);
    
//    CurrPosOut[threadId.x] = pos;
//}