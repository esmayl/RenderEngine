#pragma once
#include "Vector2D.h"

struct FoodNode
{
    Vector2D pos;
    float amount;
    bool isActive         = true; // visible/selectable; keep index stable when empty by marking inactive
    bool isTriangle       = false;
    bool isBonus          = false; // Sugar bonus: higher score, decays over time
    float scoreMultiplier = 1.0f;  // 1x normal, 2-3x for bonus nodes
    float decayPerSecond  = 0.0f;  // For bonus nodes urgency
};
