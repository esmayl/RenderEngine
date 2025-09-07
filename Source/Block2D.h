#pragma once
class Block2D
{
  public:
    explicit Block2D( int newX, int newY, int newWidth, int newHeight, float newOffset );
    int x;
    int y;
    int width;
    int height;
    float offset;
};
