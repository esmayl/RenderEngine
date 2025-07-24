#include "RandomGenerator.h"

float RandomGenerator::Generate()
{
	// 1. A hardware-based seed generator (the best source of true randomness)
	std::random_device rd;

	// 2. A random number engine, seeded with the random_device
	std::mt19937 gen(rd());

	// 3. A distribution to produce floating-point numbers in a specific range.
	//    We'll generate a random phase offset between 0 and 2*PI.
	std::uniform_real_distribution<float> distrib(0.0f, 1.0f);

	return distrib(gen);
}
