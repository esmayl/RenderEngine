#pragma once
#include "Vector2D.h"

struct Hazard
{
    Vector2D pos;
    Vector2D vel;
    float radius;
    bool active;
};
