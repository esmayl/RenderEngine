#pragma once
#include <random>

class RandomGenerator
{
	public:
		static float Generate();
		static float Slerp(float current, float start, float end);
};

