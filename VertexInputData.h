#pragma once
struct VertexInputData
{
	float sizeX; // 4 bytes
	float sizeY; // 4 bytes
	float objectPosX; // 4 bytes
	float objectPosY; // 4 bytes
	float aspectRatio;
	float time;
	int indexesX;
	int indexesY;
	float speed;
	float padding;
	float padding2;
	float padding3;
}; // Total needs to be multiples of 16 bytes otherwise buffer cannot be created