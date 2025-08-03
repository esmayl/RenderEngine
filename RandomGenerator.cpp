#include "RandomGenerator.h"

float RandomGenerator::Generate(float min, float max)
{
	// 1. A hardware-based seed generator (the best source of true randomness)
	std::random_device rd;

	// 2. A random number engine, seeded with the random_device
	std::mt19937 gen(rd());

	// 3. A distribution to produce floating-point numbers in a specific range.
	//    We'll generate a random phase offset between 0 and 1.
	std::uniform_real_distribution<float> distrib(min, max);

	return distrib(gen);
}

float RandomGenerator::Slerp(float current, float start, float end)
{
	float angleInRadials = 15 * 57.3f; // since 2 * pi radials = 360 degress
	float q1 = start;
	float q2 = end;
	float part1 = sin((1 - current) * angleInRadials) / sin(angleInRadials) * q1;
	float part2 = sin(current * angleInRadials) / sin(angleInRadials) * q2;
	return part1+part2;
}
