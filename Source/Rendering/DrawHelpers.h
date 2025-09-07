#pragma once

#include "Objects/SquareMesh.h"
#include "VertexInputData.h"

#include <d3d11.h>

namespace DrawHelpers
{
    inline void SetupVerticesAndShaders( ID3D11DeviceContext* ctx, UINT& stride, UINT& offset, UINT bufferCount,
                                         Mesh* mesh, ID3D11VertexShader* vs, ID3D11PixelShader* ps )
    {
        ctx->IASetVertexBuffers( 0, bufferCount, &mesh->vertexBuffer, &stride, &offset );
        ctx->IASetIndexBuffer( mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
        ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        ctx->VSSetShader( vs, nullptr, 0 );
        ctx->PSSetShader( ps, nullptr, 0 );
    }

    inline void UpdateAndBindVSConstants( ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const VertexInputData& data )
    {
        ctx->UpdateSubresource( cb, 0, nullptr, &data, 0, 0 );
        ctx->VSSetConstantBuffers( 0, 1, &cb );
    }
} // namespace DrawHelpers
