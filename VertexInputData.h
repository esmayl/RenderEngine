#pragma once
struct VertexInputData
{
	float size; // 4 bytes
	float objectPosX; // 4 bytes
	float objectPosY; // 4 bytes
	float aspectRatio; // 4 bytes
	float time;
	float offset;
	float padding2;
	float padding3;
}; // Total needs to be multiples of 16 bytes otherwise buffer cannot be created