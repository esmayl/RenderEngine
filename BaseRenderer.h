#pragma once
#include <Windows.h>
#include <d2d1.h>

#pragma comment(lib, "d2d1")

class BaseRenderer
{

	public:
		virtual void Init(HWND windowHandle, int blockWidth, int blockHeight){}
		virtual void OnPaint(HWND windowHandle) {}
		virtual void OnResize(int width, int height) {}
		virtual void CountFps() {}
		virtual void OnShutdown() {}
		virtual ~BaseRenderer() {}
};

