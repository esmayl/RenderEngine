#pragma once
struct Vector2D
{
	float x;
	float y;

	Vector2D()
	{
		x = 0.0f;
		y = 0.0f;
	}

	Vector2D(float newX,float newY)
	{
		x = newX;
		y = newY;
	}

	Vector2D(int newX, int newY)
	{
		x = (float)newX;
		y = (float)newY;
	}
};