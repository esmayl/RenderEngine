#pragma once
#include <random>

class RandomGenerator
{
	public:
		static float Generate(float min, float max);
		static float Slerp(float current, float start, float end);
};

