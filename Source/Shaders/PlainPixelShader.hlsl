struct VS_OUTPUT // Needs to be defined in PS as well, with matching semantics
{
	float4 position : SV_POSITION; // This is the interpolated screen-space position
	float4 color : COLOR;         // This is the interpolated color
};
float4 main(VS_OUTPUT input) : SV_TARGET {
	return input.color; // White color
};