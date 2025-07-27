#include "Utilities.h"

std::vector<Block2D> Utilities::CreateBlocks(int totalWidth, int totalHeight, int columns, int rows)
{
    std::vector<Block2D> tempBlocks;

    // Prevent division by zero if columns or rows are 0
    if(columns == 0 || rows == 0)
    {
        return tempBlocks;
    }


    // Loop through the desired number of columns and rows
    for(int i = 0; i < columns; i++)
    {
        for(int j = 0; j < rows; j++)
        {
            // Calculate the center position of the block
            int centerX = i;
            int centerY = j;

            // Create the block with the calculated position and size
            tempBlocks.push_back(Block2D(centerX, centerY, 0, 0, RandomGenerator::Generate()));
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
