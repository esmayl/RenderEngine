#pragma once
#include <vector>
#include <windows.h>
#include "Block.h"
#include "RandomGenerator.h"

class Utilities
{
	public:
		static std::vector<Block> CreateBlocks(int width, int height);
		static void CustomDrawText(HDC buffer, const wchar_t textToDraw[]);
};

