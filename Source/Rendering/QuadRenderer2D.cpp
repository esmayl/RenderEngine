#include "QuadRenderer2D.h"

#include "Rendering/DrawHelpers.h"

void QuadRenderer2D::DrawColoredQuadTL( ID3D11DeviceContext* ctx, ID3D11InputLayout* layout, Mesh* mesh,
                                        ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11Buffer* vsConstants,
                                        float aspectRatioX, const Vector2D& ndcTopLeft, float sizeX, float sizeY,
                                        int colorCode, int lightenLevel )
{
    if ( !ctx || !layout || !mesh || !vs || !ps || !vsConstants )
        return;

    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    ctx->IASetInputLayout( layout );
    DrawHelpers::SetupVerticesAndShaders( ctx, stride, offset, 1, mesh, vs, ps );

    VertexInputData cb = {};
    cb.aspectRatio     = aspectRatioX;
    cb.sizeX           = sizeX;
    cb.sizeY           = sizeY;
    cb.objectPosX      = ndcTopLeft.x;
    cb.objectPosY      = ndcTopLeft.y;
    cb.indexesX        = colorCode;
    cb.indexesY        = lightenLevel;

    DrawHelpers::UpdateAndBindVSConstants( ctx, vsConstants, cb );
    ctx->DrawIndexed( mesh->indexCount, 0, 0 );
}
