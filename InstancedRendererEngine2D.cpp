#include "InstancedRendererEngine2D.h"

InstancedRendererEngine2D::InstancedRendererEngine2D(int blockWidth, int blockHeight)
{
	blocks = Utilities::CreateBlocks(blockWidth, blockHeight);
}

void InstancedRendererEngine2D::Init(HWND windowHandle)
{
	HRESULT hr = S_OK;

	// Create the Device and Swap Chain
	DXGI_SWAP_CHAIN_DESC sd = {};

	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = windowHandle;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;

	// Create the device, device context, and swap chain.
	hr = D3D11CreateDeviceAndSwapChain(
		nullptr,                    // Default adapter
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,                    // No software device
		0,                          // No creation flags
		nullptr,                    // Default feature levels
		0,                          // Number of feature levels
		D3D11_SDK_VERSION,
		&sd,
		&pSwapChain,
		&pDevice,
		nullptr,                    // Don't need feature level
		&pDeviceContext
	);

	if(FAILED(hr)) return;

	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	
	if(FAILED(hr)) return;

	hr = pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView);

	if(FAILED(hr)) return;

	pDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

	// Get the window's client area size to set the viewport.
	RECT rc;
	GetClientRect(windowHandle, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	// Set up the viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	pDeviceContext->RSSetViewports(1, &vp);
}

void InstancedRendererEngine2D::OnPaint(HWND windowHandle)
{
	CountFps();

	if(!pDeviceContext || !pSwapChain)
	{
		return;
	}

	// Define a color to clear the window to.
	const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // A nice blue

	// Clear the back buffer.
	pDeviceContext->ClearRenderTargetView(renderTargetView, clearColor);

	// Present the back buffer to the screen.
	// The first parameter (1) enables V-Sync, locking the frame rate to the monitor's refresh rate.
	// Change to 0 to disable V-Sync.
	pSwapChain->Present(1, 0);
}

void InstancedRendererEngine2D::CountFps()
{
	auto frameStartTime = std::chrono::steady_clock::now();

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
}

void InstancedRendererEngine2D::OnShutdown()
{
}
