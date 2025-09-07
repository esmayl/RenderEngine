#pragma once

#include <d3d11.h>

// Small wrapper for commonly reused D3D11 states
class RenderStates
{
  public:
    ~RenderStates()
    {
        Release();
    }

    void CreateAlphaBlend( ID3D11Device* device )
    {
        ReleaseAlphaBlend();
        D3D11_BLEND_DESC bd                      = {};
        bd.AlphaToCoverageEnable                 = FALSE;
        bd.IndependentBlendEnable                = FALSE;
        bd.RenderTarget[0].BlendEnable           = TRUE;
        bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState( &bd, &alphaBlend );
    }

    void ApplyAlphaBlend( ID3D11DeviceContext* ctx )
    {
        if ( !alphaBlend )
            return;
        const float blendFactor[4] = { 0, 0, 0, 0 };
        ctx->OMSetBlendState( alphaBlend, blendFactor, 0xFFFFFFFF );
    }

    void Release()
    {
        ReleaseAlphaBlend();
    }

  private:
    void ReleaseAlphaBlend()
    {
        if ( alphaBlend )
        {
            alphaBlend->Release();
            alphaBlend = nullptr;
        }
    }

    ID3D11BlendState* alphaBlend = nullptr;
};
