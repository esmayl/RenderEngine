#include "Utilities.h"

std::vector<Block> Utilities::CreateBlocks(int width, int height)
{
	std::vector<Block> tempBlocks;

	for(size_t i = 0; i < 5; i++)
	{
		for(size_t j = 0; j < 5; j++)
		{

			tempBlocks.push_back(Block((width * i) + width / 2, (height * j) + height / 2, width, height, RandomGenerator::Generate()));
		}
	}

	return tempBlocks;
}

void Utilities::CustomDrawText(HDC buffer, const wchar_t textToDraw[])
{
	// Set the text color (COLORREF is a macro for RGB)
	SetTextColor(buffer, RGB(0, 0, 0)); // Black text

	// Set the background mode to transparent
	// This prevents the text from having a solid colored box behind it.
	SetBkMode(buffer, TRANSPARENT);

	// Define the rectangle where the text will be drawn
	RECT textRect;
	SetRect(&textRect, 10, 10, 300, 50); // A box at top-left: (left, top, right, bottom)

	// Draw the text
	// DT_SINGLELINE: Treats it as one line.
	// DT_LEFT: Aligns text to the left of the rectangle.
	DrawText(buffer, textToDraw, -1, &textRect, DT_SINGLELINE | DT_LEFT);
}
