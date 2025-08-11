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
	int gridX;
	int gridY;
	float padding1;

	float targetPosX;
	float targetPosY;
	float orbitDistance;
	float jitter;

	float previousTargetPosX;
	float previousTargetPosY;
	float flockTransitionTime;
	float flockFrozenTime;
}; // Total needs to be multiples of 16 bytes otherwise buffer cannot be created