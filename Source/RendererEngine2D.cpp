#include "RendererEngine2D.h"

#include <cmath>

RendererEngine2D::RendererEngine2D()
{
    D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, renderTargetFactory.GetAddressOf() );
    DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof( IDWriteFactory ),
                         reinterpret_cast<IUnknown**>( writeFactory.GetAddressOf() ) );
    startTime = std::chrono::steady_clock::now();
}

void RendererEngine2D::Init( HWND windowHandle, int blockWidth, int blockHeight )
{
    HRESULT hr = S_OK;

    RECT renderRect;
    GetClientRect( windowHandle, &renderRect );

    blocks = Utilities::CreateBlocks( renderRect.right, renderRect.bottom, renderRect.right / blockWidth,
                                      renderRect.bottom / blockHeight );

    D2D1_SIZE_U renderSize = D2D1::SizeU( renderRect.right, renderRect.bottom );

    // Overwrite vsync
    renderOverrides = D2D1::HwndRenderTargetProperties( windowHandle, renderSize, D2D1_PRESENT_OPTIONS_IMMEDIATELY );

    hr = renderTargetFactory->CreateHwndRenderTarget( D2D1::RenderTargetProperties(), renderOverrides,
                                                      renderTarget.ReleaseAndGetAddressOf() );

    // Micro optimization to not re-create every frame
    if ( !brush )
    {
        const D2D1_COLOR_F color = D2D1::ColorF( 1.0f, 1.0f, 1.0f, 1.0f );
        hr                       = renderTarget->CreateSolidColorBrush( color, brush.ReleaseAndGetAddressOf() );
    }

    if ( !textFormat )
    {
        hr = writeFactory->CreateTextFormat( L"Gabriola", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, 36.0f, L"en-us",
                                             textFormat.ReleaseAndGetAddressOf() );

        textRect.left   = 10;
        textRect.right  = 500;
        textRect.bottom = 300;
        textRect.top    = 0;
    }
}

HRESULT RendererEngine2D::SetupRenderTarget( HWND windowHandle )
{
    HRESULT hr = S_OK;

    if ( !renderTarget )
    {
        RECT renderRect;
        GetClientRect( windowHandle, &renderRect );

        D2D1_SIZE_U renderSize = D2D1::SizeU( renderRect.right, renderRect.bottom );

        // Overwrite vsync
        renderOverrides =
            D2D1::HwndRenderTargetProperties( windowHandle, renderSize, D2D1_PRESENT_OPTIONS_IMMEDIATELY );

        hr = renderTargetFactory->CreateHwndRenderTarget( D2D1::RenderTargetProperties(), renderOverrides,
                                                          renderTarget.ReleaseAndGetAddressOf() );
    }

    return hr;
}

void RendererEngine2D::OnPaint( HWND windowHandle )
{
    CountFps();

    HRESULT hr = SetupRenderTarget( windowHandle );

    if ( hr >= 0 )
    {
        renderTarget->BeginDraw();

        renderTarget->Clear( D2D1::ColorF( D2D1::ColorF::SkyBlue ) );

        // Get the current time point
        auto currentTime                              = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = currentTime - startTime;

        // Recreate the path geometry each frame
        batchPathGeometry.Reset();
        hr = renderTargetFactory->CreatePathGeometry( batchPathGeometry.ReleaseAndGetAddressOf() );

        if ( hr >= 0 )
        {
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> geometrySink;
            hr = batchPathGeometry->Open( geometrySink.GetAddressOf() );

            if ( hr >= 0 )
            {
                for ( const auto& b : blocks )
                {
                    const float sinnedY =
                        static_cast<float>( b.y + std::sin( elapsed_seconds.count() * 5 + b.offset ) * 5 );
                    const float blockX           = static_cast<float>( b.x );
                    const D2D1_POINT_2F topLeft  = D2D1::Point2F( blockX, sinnedY );
                    const D2D1_POINT_2F lines[3] = { D2D1::Point2F( blockX + b.width, sinnedY ),
                                                     D2D1::Point2F( blockX + b.width, sinnedY + b.height ),
                                                     D2D1::Point2F( blockX, sinnedY + b.height ) };

                    geometrySink->BeginFigure( topLeft, D2D1_FIGURE_BEGIN_FILLED );
                    geometrySink->AddLines( lines, 3 );
                    geometrySink->EndFigure( D2D1_FIGURE_END_CLOSED );
                }

                hr = geometrySink->Close();
            }

            if ( hr >= 0 )
            {
                renderTarget->FillGeometry( batchPathGeometry.Get(), brush.Get() );
            }
        }
        renderTarget->DrawTextW( fpsText, static_cast<UINT32>( wcslen( fpsText ) ), textFormat.Get(), textRect,
                                 brush.Get() );
        hr = renderTarget->EndDraw();

        if ( FAILED( hr ) || hr == D2DERR_RECREATE_TARGET )
        {
            ReleaseRenderTarget();
        }
    }
}

void RendererEngine2D::CountFps()
{
    auto frameStartTime = std::chrono::steady_clock::now();

    auto currentTime = std::chrono::steady_clock::now();
    deltaTime        = std::chrono::duration<double>( currentTime - lastFrameTime ).count();
    lastFrameTime    = currentTime;

    timeSinceFPSUpdate += deltaTime;
    framesSinceFPSUpdate++;

    if ( timeSinceFPSUpdate >= 1.0 )
    {
        const double currentFPS = framesSinceFPSUpdate / timeSinceFPSUpdate;
        swprintf_s( fpsText, L"FPS: %.0f", currentFPS );

        // Reset for the next second
        timeSinceFPSUpdate   = 0.0;
        framesSinceFPSUpdate = 0;
    }
}

void RendererEngine2D::OnShutdown()
{
    renderTargetFactory.Reset();
    batchPathGeometry.Reset();
    textFormat.Reset();
    ReleaseRenderTarget();
}

void RendererEngine2D::ReleaseRenderTarget()
{
    renderTarget.Reset();
    brush.Reset();
}
