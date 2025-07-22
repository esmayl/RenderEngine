#pragma once

#include <chrono>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>

#include "Utilities.h"
#include "Block2D.h"
#include "BaseRenderer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

class InstancedRendererEngine2D : public BaseRenderer
{
	public:
		explicit InstancedRendererEngine2D(int blockWidth, int blockHeight);
		void Init(HWND windowHandle) override;
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

		IDXGISwapChain* pSwapChain;
		ID3D11Device* pDevice;
		ID3D11DeviceContext* pDeviceContext;
		ID3D11RenderTargetView* renderTargetView;
		ID3D11Texture2D* pBackBuffer;

};