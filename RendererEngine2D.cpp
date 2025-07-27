#include "RendererEngine2D.h"

RendererEngine2D::RendererEngine2D()
{
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &renderTargetFactory);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&writeFactory));

	startTime = std::chrono::steady_clock::now();
}

void RendererEngine2D::Init(HWND windowHandle, int blockWidth, int blockHeight)
{
	HRESULT hr = S_OK;

	RECT renderRect;
	GetClientRect(windowHandle, &renderRect);

	blocks = Utilities::CreateBlocks(renderRect.right, renderRect.bottom, renderRect.right / blockWidth, renderRect.bottom / blockHeight);

	D2D1_SIZE_U renderSize = D2D1::SizeU(renderRect.right, renderRect.bottom);

	// Overwrite vsync
	renderOverrides = D2D1::HwndRenderTargetProperties(
		windowHandle,
		renderSize,
		D2D1_PRESENT_OPTIONS_IMMEDIATELY
	);

	hr = renderTargetFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		renderOverrides,
		&renderTarget
	);

	// Micro optimization to not re-create every frame
	if(pBrush == nullptr)
	{
		const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
		hr = renderTarget->CreateSolidColorBrush(color, &pBrush);
	}

	if(textFormat == nullptr)
	{
		hr = writeFactory->CreateTextFormat(
			L"Gabriola",
			NULL,
			DWRITE_FONT_WEIGHT_REGULAR,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			36.0f,
			L"en-us",
			&textFormat
		);

		textRect.left = 10;
		textRect.right = 500;
		textRect.bottom = 300;
		textRect.top = 0;
	}
}

HRESULT RendererEngine2D::SetupRenderTarget(HWND windowHandle)
{
	HRESULT hr = S_OK;

	if(renderTarget == nullptr)
	{
		RECT renderRect;
		GetClientRect(windowHandle, &renderRect);

		D2D1_SIZE_U renderSize = D2D1::SizeU(renderRect.right, renderRect.bottom);

		// Overwrite vsync
		renderOverrides = D2D1::HwndRenderTargetProperties(
			windowHandle,
			renderSize,
			D2D1_PRESENT_OPTIONS_IMMEDIATELY
		);

		hr = renderTargetFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			renderOverrides,
			&renderTarget
		);
	}

	return hr;
}

void RendererEngine2D::OnPaint(HWND windowHandle)
{
	CountFps();

	HRESULT hr = SetupRenderTarget(windowHandle);

	if(hr >= 0)
	{
		renderTarget->BeginDraw();

		renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));

		// Get the current time point
		auto currentTime = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed_seconds = currentTime - startTime;

		if(batchPathGeometry != nullptr)
		{
			(batchPathGeometry)->Release();
			batchPathGeometry = NULL;
		}

		// Re init the path geometry
		hr = renderTargetFactory->CreatePathGeometry(&batchPathGeometry);

		if(hr >= 0)
		{
			ID2D1GeometrySink* geometrySink = NULL;

			hr = batchPathGeometry->Open(&geometrySink);

			if(hr >= 0)
			{
				int sinnedY;
				int edgeFlags = EDGE_RAISED;
				for(size_t i = 0; i < blocks.size(); i++)
				{
					sinnedY = (int)(blocks[i].y + sin(elapsed_seconds.count() * 5 + blocks[i].offset) * 5);
					D2D1_POINT_2F topLeft = D2D1::Point2F(blocks[i].x, sinnedY);
					D2D1_POINT_2F lines[3] = {
						D2D1::Point2F(blocks[i].x + blocks[i].width, sinnedY),
						D2D1::Point2F(blocks[i].x + blocks[i].width, sinnedY + blocks[i].height),
						D2D1::Point2F(blocks[i].x, sinnedY + blocks[i].height)
					};

					geometrySink->BeginFigure(topLeft,D2D1_FIGURE_BEGIN_FILLED);

					geometrySink->AddLines(lines,3);

					geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
				}

				hr = geometrySink->Close();
			}

			if(&geometrySink)
			{
				(geometrySink)->Release();
				geometrySink = NULL;
			}

			if(hr >= 0)
			{
				renderTarget->FillGeometry(batchPathGeometry,pBrush);
			}
		}
		//Utilities::CustomDrawText(backBufferHdc, fpsText);

		renderTarget->DrawTextW(fpsText, (UINT32)wcslen(fpsText), textFormat, textRect, pBrush);
		hr = renderTarget->EndDraw();

		if(FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			ReleaseRenderTarget();
		}
	}
}

void RendererEngine2D::CountFps()
{
	auto frameStartTime = std::chrono::steady_clock::now();

	auto currentTime = std::chrono::steady_clock::now();
	deltaTime = std::chrono::duration<double>(currentTime - lastFrameTime).count();
	lastFrameTime = currentTime;

	timeSinceFPSUpdate += deltaTime;
	framesSinceFPSUpdate++;

	if(timeSinceFPSUpdate >= 1.0)
	{
		double currentFPS = framesSinceFPSUpdate / timeSinceFPSUpdate;
		swprintf_s(fpsText, L"FPS: %.0f", currentFPS); // Update the global text buffer

		// Reset for the next second
		timeSinceFPSUpdate = 0.0;
		framesSinceFPSUpdate = 0;
	}
}

void RendererEngine2D::OnShutdown()
{
	if(&renderTargetFactory)
	{
		(renderTargetFactory)->Release();
		renderTargetFactory = NULL;
	}

	if(&batchPathGeometry)
	{
		(batchPathGeometry)->Release();
		batchPathGeometry = NULL;
	}

	if(&textFormat)
	{
		(textFormat)->Release();
		textFormat = NULL;
	}

	ReleaseRenderTarget();
}

void RendererEngine2D::ReleaseRenderTarget()
{
	if(&renderTarget)
	{
		(renderTarget)->Release();
		renderTarget = NULL;
	}

	if(&pBrush)
	{
		(pBrush)->Release();
		pBrush = NULL;
	}
}
