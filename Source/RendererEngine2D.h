#pragma once
#include "BaseRenderer.h"
#include "Block2D.h"
#include "Utilities.h"

#include <Windows.h>
#include <chrono>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <wrl/client.h>

#pragma comment( lib, "d2d1" )
#pragma comment( lib, "dwrite" )

class RendererEngine2D : public BaseRenderer
{
  public:
    RendererEngine2D();
    void Init( HWND windowHandle, int blockWidth, int blockHeight ) override;
    void OnPaint( HWND windowHandle ) override;
    void CountFps() override;
    void OnShutdown() override;

  private:
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    std::chrono::time_point<std::chrono::steady_clock> lastFrameTime;
    double deltaTime          = 0;
    double timeSinceFPSUpdate = 0.0;
    int framesSinceFPSUpdate  = 0;
    wchar_t fpsText[9]{};

    std::vector<Block2D> blocks;

    Microsoft::WRL::ComPtr<ID2D1Factory> renderTargetFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> batchPathGeometry;

    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;

    D2D1_HWND_RENDER_TARGET_PROPERTIES renderOverrides{};

    D2D1_RECT_F textRect{};

    HRESULT SetupRenderTarget( HWND windowHandle );
    void ReleaseRenderTarget();
};
