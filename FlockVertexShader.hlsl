cbuffer TextInputData : register(b0)
{
    float2 size;
    float2 objectPos;
    float2 screenSize;
    float2 uvOffset;
    float2 uvScale;
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
        
    float3 finalPos = input.pos + float3(input.instancePos, 0.0f);
    output.position = float4(finalPos, 1.0f);
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	return output;
}