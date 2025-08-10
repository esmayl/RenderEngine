#include "Main.h"

int blockWidth = 1;
int blockHeight = 1;

InstancedRendererEngine2D renderEngine;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_CREATE:
			// Init the render target factory
			renderEngine.Init(hwnd, blockWidth, blockHeight);
			return 0;

		case WM_DESTROY:
			renderEngine.OnShutdown();
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			renderEngine.OnResize(width, height);
			return 0;
		}

		case WM_LBUTTONUP:
		{
			int x = GET_X_LPARAM(lParam); // signed int to take into account minus values
			int y = GET_Y_LPARAM(lParam); // signed int to take into account minus values
			renderEngine.SetFlockTarget(x,y);
			return 0;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			renderEngine.OnPaint(hwnd);

			EndPaint(hwnd, &ps);
		}

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	// Request higher timer precision from Windows
	timeBeginPeriod(1);

	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	// Window class ?? 
	WNDCLASS wc = { };

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	// Register the window class
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Learn to Program Windows",    // Window text
		WS_OVERLAPPEDWINDOW,            // Window style
		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, 
		1280, 720, // Window size
		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	// If we cant create the window we return
	if(hwnd == NULL)
	{
		timeEndPeriod(1);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);

	MSG msg = { };


	bool quitGame = false;

	while(!quitGame)
	{
		// Process all pending messages in the queue.
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Check for the quit message.
			if(msg.message == WM_QUIT)
			{
				quitGame = true;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if(quitGame)
		{
			break;
		}

		InvalidateRect(hwnd, NULL, FALSE);

		//// --- Frame Rate Limiter (No changes needed here) ---
		//auto frameEndTime = std::chrono::steady_clock::now();
		//auto frameDuration = frameEndTime - currentTime;
		//if(frameDuration < TARGET_FRAME_TIME)
		//{
		//	std::this_thread::sleep_for(TARGET_FRAME_TIME - frameDuration);
		//}
	}

	timeEndPeriod(1);
	return 0;
}

