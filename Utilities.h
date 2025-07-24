#pragma once
#include <vector>
#include <windows.h>
#include "Block2D.h"
#include "RandomGenerator.h"

class Utilities
{
	public:
		static std::vector<Block2D> CreateBlocks(int totalWidth, int totalHeight, int columns, int rows);
		static void CustomDrawText(HDC buffer, const wchar_t textToDraw[]);
};

