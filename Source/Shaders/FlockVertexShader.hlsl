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
    int3 _paddingV;
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

StructuredBuffer<InstanceData> CurrPos : register(t0); // same as CurrPosIn after swap

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
    float2 finalPos = quad + float2(CurrPos[input.instanceId].x, CurrPos[input.instanceId].y);
    
    output.position = float4(finalPos, 0.0f, 1.0f);
    
    // Visual cue:
    // - ToFood ants heading to current active food: yellow
    // - ToFood ants heading to old food: orange
    // - ToNest ants: cyan
    int state = CurrPos[input.instanceId].movementState;
    float2 goal = float2(CurrPos[input.instanceId].goalX, CurrPos[input.instanceId].goalY);
    bool towardCurrent = distance(goal, targetPos) < 1e-3f;
    if (state == 0)
    {
        output.color = towardCurrent ? float4(1.0f, 0.9f, 0.2f, 1.0f)
                                     : float4(1.0f, 0.6f, 0.2f, 1.0f);
    }
    else
    {
        output.color = float4(0.2f, 0.9f, 0.9f, 1.0f);
    }
    return output;
}
