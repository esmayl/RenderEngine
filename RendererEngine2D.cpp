#include "RendererEngine2D.h"

RendererEngine2D::RendererEngine2D(int blockWidth,int blockHeight)
{
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &renderTargetFactory);
	blocks = Utilities::CreateBlocks(blockWidth, blockHeight);
}

HRESULT RendererEngine2D::SetupRenderTarget(HWND windowHandle)
{
	HRESULT hr = S_OK;

	if(renderTarget == nullptr)
	{
		RECT renderRect;
		GetClientRect(windowHandle, &renderRect);

		D2D1_SIZE_U renderSize = D2D1::SizeU(renderRect.right, renderRect.bottom);

		hr = renderTargetFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(windowHandle, renderSize),
			&renderTarget
		);

		if(hr >= 0)
		{
			// Micro optimization to not re-create every frame
			if(pBrush == nullptr)
			{
				const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
				hr = renderTarget->CreateSolidColorBrush(color, &pBrush);
			}
		}
	}

	return hr;
}

void RendererEngine2D::OnPaint(HWND windowHandle)
{
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
				D2D1_RECT_F edgeRect;
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

		hr = renderTarget->EndDraw();

		if(FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			ReleaseRenderTarget();
		}
	}
}

void RendererEngine2D::OnShutdown()
{
	if(&renderTargetFactory)
	{
		(renderTargetFactory)->Release();
		renderTargetFactory = NULL;
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
