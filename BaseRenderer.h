#pragma once
#include <Windows.h>
#include <d2d1.h>

#pragma comment(lib, "d2d1")

class BaseRenderer
{

	public:
		virtual void OnPaint(HWND windowHandle) {}
		virtual void CountFps() {}
		virtual void OnShutdown() {}
		virtual ~BaseRenderer() {}
};

