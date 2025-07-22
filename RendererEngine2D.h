#pragma once
#include <chrono>
#include <vector>
#include <Windows.h>
#include <d2d1.h>
#include "Block2D.h"

#include "Utilities.h"

#pragma comment(lib, "d2d1")

class RendererEngine2D
{
	public:
		ID2D1Factory* renderTargetFactory = nullptr;
		ID2D1HwndRenderTarget* renderTarget = nullptr;
		ID2D1SolidColorBrush* pBrush = nullptr;

		// Define our target frame rate (e.g., 60 FPS)
		const int TARGET_FPS = 60;

		// Calculate the time one frame should take
		const std::chrono::duration<long long,std::milli> TARGET_FRAME_TIME = std::chrono::milliseconds(1000) / TARGET_FPS;

		std::vector<Block2D> blocks;

		explicit RendererEngine2D(int blockWidth,int blockHeight);
		void OnPaint(HWND windowHandle);
		void OnShutdown();

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		wchar_t fpsText[256];
		ID2D1PathGeometry* batchPathGeometry = nullptr;

		HRESULT SetupRenderTarget(HWND windowHandle);
		void ReleaseRenderTarget();
};

