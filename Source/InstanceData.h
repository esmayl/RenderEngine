#pragma once

#include "DirectXMath.h"
struct InstanceData
{
    float posX;
    float posY;
    float directionX;
    float directionY;
    DirectX::XMFLOAT4 color;
    int movementState;
};