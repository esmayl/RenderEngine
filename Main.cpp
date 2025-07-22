#include "Main.h"


double deltaTime = 0;
int width = 100;
int height = 100;


RendererEngine2D renderEngine;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_CREATE:
			// Init the render target factory
			D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &renderEngine.renderTargetFactory);
			renderEngine.blocks = Utilities::CreateBlocks(width, height);
			return 0;

		case WM_DESTROY:
			renderEngine.OnShutdown();
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
		{
			// Get a device context for the window
			HDC hdc = GetDC(hwnd);

			// Release the temporary DC
			ReleaseDC(hwnd, hdc);
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
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
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

	auto lastFrameTime = std::chrono::steady_clock::now();
	double timeSinceFPSUpdate = 0.0;
	int framesSinceFPSUpdate = 0;

	bool quitGame = false;

	while(!quitGame)
	{
		auto frameStartTime = std::chrono::steady_clock::now();

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

		// --- Time and FPS Calculation (The Right Way) ---
		auto currentTime = std::chrono::steady_clock::now();
		deltaTime = std::chrono::duration<double>(currentTime - lastFrameTime).count();
		lastFrameTime = currentTime;

		timeSinceFPSUpdate += deltaTime;
		framesSinceFPSUpdate++;

		if(timeSinceFPSUpdate >= 1.0)
		{
			//double currentFPS = framesSinceFPSUpdate / timeSinceFPSUpdate;
			//swprintf_s(fpsText, L"FPS: %.0f", currentFPS); // Update the global text buffer

			// Reset for the next second
			timeSinceFPSUpdate = 0.0;
			framesSinceFPSUpdate = 0;
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

