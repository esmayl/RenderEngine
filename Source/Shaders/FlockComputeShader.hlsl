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
    int activeFoodIndex;
    int3 _paddingVc;
}

struct InstanceData
{
    float x;
    float y;
    float directionX;
    float directionY;
    float goalX;
    float goalY;
    float laneOffset;
    float speedScale;
    float4 color;
    int movementState;
};

StructuredBuffer<InstanceData> CurrPosIn : register(t0);
RWStructuredBuffer<InstanceData> CurrPosOut : register(u0);

// States: 0 = ToFood, 1 = ToNest

[numthreads(256, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    uint id = threadId.x;

    float2 pos = float2(CurrPosIn[id].x, CurrPosIn[id].y);
    int state = CurrPosIn[id].movementState;
    // Default: preserve previous goal
    CurrPosOut[id].goalX = CurrPosIn[id].goalX;
    CurrPosOut[id].goalY = CurrPosIn[id].goalY;

    float stopDistance = 0.03f;

    float2 food = targetPos;              // current global food target (for newly leaving ants)
    float2 nest = previousTargetPos;      // nest position

    float2 dir = float2(0.0f, 0.0f);
    float2 newPos = pos;
    int newState = state;

    if(state == 0)
    {
        // ToFood: move towards personal goal (snapshot when leaving nest)
        float2 goal = float2(CurrPosIn[id].goalX, CurrPosIn[id].goalY);
    float2 toGoal = goal - pos;
    float d = length(toGoal);
    if(d > 1e-5f)
    {
        float step = min(d, speed * CurrPosIn[id].speedScale * deltaTime);
        float2 baseDir = toGoal / d;
        float2 side = float2(-baseDir.y, baseDir.x);
        float amp = CurrPosIn[id].laneOffset * saturate(d / 0.2f); // fade near goal
        float2 steer = normalize(baseDir + side * amp);
        dir = steer;
        newPos = pos + dir * step;
    }
        else
        {
            newPos = pos;
            dir = float2(0.0f, 0.0f);
        }

        // Arrived at food -> switch to nest
        if(distance(newPos, goal) <= stopDistance)
        {
            newState = 1;
            // on switching, set goal to nest
            CurrPosOut[id].goalX = nest.x;
            CurrPosOut[id].goalY = nest.y;
        }
    }
    else
    {
        // ToNest: move towards nest
        float2 toNest = nest - pos;
        float distNest = length(toNest);
        if(distNest > 1e-5f)
        {
            float step = min(distNest, speed * CurrPosIn[id].speedScale * deltaTime);
            float2 baseDir = toNest / distNest;
            float2 side = float2(-baseDir.y, baseDir.x);
            float amp = CurrPosIn[id].laneOffset * saturate(distNest / 0.2f);
            float2 steer = normalize(baseDir + side * amp);
            dir = steer;
            newPos = pos + dir * step;
        }
        else
        {
            dir = float2(0.0f, 0.0f);
            newPos = pos;
        }

        // Arrived at nest -> go to current global food
        if(distNest <= stopDistance)
        {
            newState = 0;
            CurrPosOut[id].goalX = food.x;
            CurrPosOut[id].goalY = food.y;
        }
        else
        {
            newState = 1;
        }
    }

    CurrPosOut[id].x = newPos.x;
    CurrPosOut[id].y = newPos.y;
    CurrPosOut[id].directionX = dir.x;
    CurrPosOut[id].directionY = dir.y;
    CurrPosOut[id].color = CurrPosIn[id].color;
    CurrPosOut[id].movementState = newState;
    CurrPosOut[id].laneOffset = CurrPosIn[id].laneOffset;
    CurrPosOut[id].speedScale = CurrPosIn[id].speedScale;
    if(state == 0)
    {
        // Keep current goal when moving to food
        if(!(distance(newPos, float2(CurrPosIn[id].goalX, CurrPosIn[id].goalY)) <= stopDistance))
        {
            CurrPosOut[id].goalX = CurrPosIn[id].goalX;
            CurrPosOut[id].goalY = CurrPosIn[id].goalY;
        }
    }
}
