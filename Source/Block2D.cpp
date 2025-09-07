#include "Block2D.h"

Block2D::Block2D( int newX, int newY, int newWidth, int newHeight, float newOffset )
{
    this->x      = newX;
    this->y      = newY;
    this->width  = newWidth;
    this->height = newHeight;
    this->offset = newOffset;
}
