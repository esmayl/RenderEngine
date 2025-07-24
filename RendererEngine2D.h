#pragma once
#include <chrono>
#include <vector>
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include "Block2D.h"
#include "Utilities.h"
#include "BaseRenderer.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

class RendererEngine2D : public BaseRenderer
{
	public:

		explicit RendererEngine2D();
		void Init(HWND windowHandle, int blockWidth, int blockHeight) override;
		void OnPaint(HWND windowHandle) override;
		void CountFps() override;
		void OnShutdown() override;

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		std::chrono::time_point<std::chrono::steady_clock> lastFrameTime;
		double deltaTime = 0;
		double timeSinceFPSUpdate = 0.0;
		int framesSinceFPSUpdate = 0;
		wchar_t fpsText[20];

		std::vector<Block2D> blocks;

		ID2D1Factory* renderTargetFactory = nullptr;
		ID2D1HwndRenderTarget* renderTarget = nullptr;
		ID2D1SolidColorBrush* pBrush = nullptr;
		ID2D1PathGeometry* batchPathGeometry = nullptr;

		IDWriteFactory* writeFactory = nullptr;
		IDWriteTextFormat* textFormat = nullptr;

		D2D1_HWND_RENDER_TARGET_PROPERTIES renderOverrides;

		D2D1_RECT_F textRect;

		HRESULT SetupRenderTarget(HWND windowHandle);
		void ReleaseRenderTarget();
};

