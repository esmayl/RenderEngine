#pragma once
class Block
{
	public:
		explicit Block(int newX, int newY, int newWidth, int newHeight, float newOffset);
		int x;
		int y;
		int width;
		int height;
		float offset;
};

