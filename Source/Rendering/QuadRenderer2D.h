#pragma once

#include "Objects/SquareMesh.h"
#include "Vector2D.h"
#include "VertexInputData.h"

#include <d3d11.h>

class QuadRenderer2D
{
  public:
    static void DrawColoredQuadTL( ID3D11DeviceContext* ctx, ID3D11InputLayout* layout, Mesh* mesh,
                                   ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11Buffer* vsConstants,
                                   float aspectRatioX, const Vector2D& ndcTopLeft, float sizeX, float sizeY,
                                   int colorCode, int lightenLevel = 0 );
};
