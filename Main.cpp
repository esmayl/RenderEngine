#include "Main.h"


std::vector<Block> blocks;
auto startTime = std::chrono::steady_clock::now();
wchar_t fpsText[256];
double deltaTime = 0;

// Define our target frame rate (e.g., 60 FPS)
const int TARGET_FPS = 60;

// Calculate the time one frame should take
const auto TARGET_FRAME_TIME = std::chrono::milliseconds(1000) / TARGET_FPS;


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HDC backBufferHdc = NULL;
	static HBITMAP backBufferBitmap = NULL;
	static HBITMAP previousFrame = NULL;
	static wchar_t timeText[256];

	switch(uMsg)
	{
		case WM_DESTROY:
			// --- Final Cleanup ---
			// Clean up the GDI objects when the window is destroyed.
			SelectObject(backBufferHdc, previousFrame);
			DeleteObject(backBufferBitmap);
			DeleteDC(backBufferHdc);

			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
		{
			// This message is sent when the window is created and resized.
			// THIS IS WHERE YOU CREATE THE BACK BUFFER.

			// Clean up old resources if they exist
			if(backBufferHdc != NULL)
			{
				SelectObject(backBufferHdc, previousFrame);
				DeleteObject(backBufferBitmap);
				DeleteDC(backBufferHdc);
			}

			// Get the new window size
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);

			// Get a device context for the window
			HDC hdc = GetDC(hwnd);

			// Create the new back buffer resources
			backBufferHdc = CreateCompatibleDC(hdc);
			backBufferBitmap = CreateCompatibleBitmap(hdc, width, height);
			previousFrame = (HBITMAP)SelectObject(backBufferHdc, backBufferBitmap);

			// Release the temporary DC
			ReleaseDC(hwnd, hdc);
			return 0;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			// Get the size of the client area of the window
			RECT clientRect;
			GetClientRect(hwnd, &clientRect);

			// All painting occurs here, between BeginPaint and EndPaint.
			FillRect(backBufferHdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 6));

			// Get the current time point
			auto currentTime = std::chrono::steady_clock::now();
			std::chrono::duration<double> elapsed_seconds = currentTime - startTime;

			RECT edgeRect;
			int sinnedY;
			int edgeFlags = EDGE_RAISED;
			for(size_t i = 0; i < blocks.size(); i++)
			{
				sinnedY = (int)(blocks[i].y + sin(elapsed_seconds.count() * 5 + blocks[i].offset) * 5);
				edgeRect.left = blocks[i].x;
				edgeRect.top = sinnedY;
				edgeRect.right = blocks[i].x + blocks[i].width;
				edgeRect.bottom = sinnedY + blocks[i].height;

				DrawEdge(backBufferHdc, &edgeRect ,edgeFlags, BF_RECT);
			}

			Utilities::CustomDrawText(backBufferHdc, fpsText);

			BitBlt(hdc,0,0, clientRect.right, clientRect.bottom, backBufferHdc,0,0,SRCCOPY);

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

	int width = 100;
	int height = 100;

	blocks = Utilities::CreateBlocks(width, height);

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
			double currentFPS = framesSinceFPSUpdate / timeSinceFPSUpdate;
			swprintf_s(fpsText, L"FPS: %.0f", currentFPS); // Update the global text buffer

			// Reset for the next second
			timeSinceFPSUpdate = 0.0;
			framesSinceFPSUpdate = 0;
		}


		InvalidateRect(hwnd, NULL, FALSE);

		// --- Frame Rate Limiter (No changes needed here) ---
		auto frameEndTime = std::chrono::steady_clock::now();
		auto frameDuration = frameEndTime - currentTime;
		if(frameDuration < TARGET_FRAME_TIME)
		{
			std::this_thread::sleep_for(TARGET_FRAME_TIME - frameDuration);
		}
	}

	timeEndPeriod(1);
	return 0;
}

