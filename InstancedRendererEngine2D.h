#pragma once

#include <chrono>
#include "BaseRenderer.h"

class InstancedRendererEngine2D : public BaseRenderer
{
	public:
		explicit InstancedRendererEngine2D(int blockWidth, int blockHeight);
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
};