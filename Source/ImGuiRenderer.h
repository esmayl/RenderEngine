#pragma once
#include "imgui.h"

#include <d3d11.h>
#include <windows.h>

class ImGuiRenderer
{
  public:
    bool Init( HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context );
    void Shutdown();

    void NewFrame( float displayWidth, float displayHeight, double deltaTime );
    void Render();

    void ProcessWin32Event( UINT msg, WPARAM wParam, LPARAM lParam );

  private:
    void CreateDeviceObjects();
    void InvalidateDeviceObjects();
    void UpdateFontTexture();

    HWND m_hwnd                = nullptr;
    ID3D11Device* m_device     = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;

    ID3D11Buffer* m_vb = nullptr;
    ID3D11Buffer* m_ib = nullptr;
    int m_vbSize       = 0;
    int m_ibSize       = 0;

    ID3D11VertexShader* m_vs            = nullptr;
    ID3D11PixelShader* m_ps             = nullptr;
    ID3D11InputLayout* m_layout         = nullptr;
    ID3D11Buffer* m_cb                  = nullptr; // projection
    ID3D11SamplerState* m_sampler       = nullptr;
    ID3D11ShaderResourceView* m_fontSRV = nullptr;
    ID3D11BlendState* m_blend           = nullptr;
    ID3D11RasterizerState* m_raster     = nullptr;
    ID3D11DepthStencilState* m_depth    = nullptr;
};
