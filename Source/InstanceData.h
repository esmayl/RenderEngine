#pragma once

#include "DirectXMath.h"
struct InstanceData
{
    float posX;
    float posY;
    float directionX;
    float directionY;
    float goalX;
    float goalY;
    float laneOffset;   // perpendicular offset magnitude
    float speedScale;   // per-ant speed multiplier
    DirectX::XMFLOAT4 color;
    int movementState;
};
