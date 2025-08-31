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
    float holdTimer;    // seconds to wait at nest before departing
    DirectX::XMFLOAT4 color;
    int movementState;
    int sourceIndex;
};
