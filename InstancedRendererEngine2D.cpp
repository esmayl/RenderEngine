#include "InstancedRendererEngine2D.h"

template<class T>
void SafeRelease(T** ppT)
{
	if(*ppT)
	{
		(*ppT)->Release();
		*ppT = nullptr;
	}
}

void InstancedRendererEngine2D::Init(HWND windowHandle, int blockWidth, int blockHeight)
{
	HRESULT hr = S_OK;
	startTime = std::chrono::steady_clock::now();

	// Create the Device and Swap Chain
	DXGI_SWAP_CHAIN_DESC sd = {};

	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = windowHandle;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;

	// Manually set all 'feature' levels, basically the directx versions. Also check project settings and hlsl setting per file for compiling model etc..
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	// Create the device, device context, and swap chain.
	hr = D3D11CreateDeviceAndSwapChain(
		nullptr,                    // Default adapter
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,                    // No software device
		0,                          // No creation flags
		featureLevels,                    // Default feature levels
		numFeatureLevels,                          // Number of feature levels
		D3D11_SDK_VERSION,
		&sd,
		&pSwapChain,
		&pDevice,
		nullptr,                    // Don't need feature level
		&pDeviceContext
	);
	if(FAILED(hr)) return;

	InitRenderBufferAndTargetView(hr);
	if(FAILED(hr)) return;

	// Get the window's client area size to set the viewport.
	RECT rc;
	GetClientRect(windowHandle, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;
	
	//blocks = Utilities::CreateBlocks(width, height,width / blockWidth, height / blockHeight);

	SetupViewport(width, height);

	CreateShaders();
	CreateBuffers();
}

void InstancedRendererEngine2D::SetupViewport(UINT width, UINT height)
{
	// Set up the viewport
	D3D11_VIEWPORT vp = {};

	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	pDeviceContext->RSSetViewports(1, &vp);

	// Used for correcting triangles to their original shape and ignoring the aspect ratio of the viewport
	aspectRatioX = (height > 0) ? (FLOAT)height / (FLOAT)width : 1.0f;
}

void InstancedRendererEngine2D::InitRenderBufferAndTargetView(HRESULT& hr)
{
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if(FAILED(hr)) return;

	hr = pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView);
	if(FAILED(hr)) return;

	pDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
}

void InstancedRendererEngine2D::OnPaint(HWND windowHandle)
{
	CountFps();

	auto currentTime = std::chrono::steady_clock::now();
	float deltaTime = std::chrono::duration<float>(currentTime - startTime).count();
	startTime = currentTime;
	totalTime += deltaTime;

	if(!pDeviceContext || !pSwapChain)
	{
		return;
	}

	// Define a color to clear the window to.
	const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // A nice blue

	pDeviceContext->ClearRenderTargetView(renderTargetView, clearColor); // Clear the back buffer.
	pDeviceContext->IASetInputLayout(pInputLayout); // Setup input variables for the vertex shader like the vertex position

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	
	pDeviceContext->IASetVertexBuffers(0, 1, &square->renderingData->vertexBuffer, &stride, &offset);
	pDeviceContext->IASetIndexBuffer(square->renderingData->indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Define what primitive we should draw with the vertex and indices

	pDeviceContext->VSSetShader(pVertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(pPixelShader, nullptr, 0);

	RenderWavingGrid(20,20);

	// Present the back buffer to the screen.
	// The first parameter (1) enables V-Sync, locking the frame rate to the monitor's refresh rate.
	// Change to 0 to disable V-Sync.
	pSwapChain->Present(1, 0);
}

void InstancedRendererEngine2D::OnResize(int width, int height)
{
	pDeviceContext->OMSetRenderTargets(0, 0, 0);
	SafeRelease(&renderTargetView);

	pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

	HRESULT hr = S_OK;
	InitRenderBufferAndTargetView(hr);
	
	if(FAILED(hr)) return;

	SetupViewport(width,height);
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
	delete square;
}


void InstancedRendererEngine2D::CreateShaders()
{
	// --- Compile and Create Shaders ---
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	const wchar_t* vsFilePath = L"TextVertexShader.hlsl"; // Path to your HLSL file

	HRESULT hr = D3DCompileFromFile(vsFilePath, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
	if(FAILED(hr))
	{ 
		SafeRelease(&errorBlob);
		return; 
	}

	const wchar_t* psFilePath = L"PlainPixelShader.hlsl"; // Path to your HLSL file

	hr = D3DCompileFromFile(psFilePath, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
	if(FAILED(hr))
	{
		SafeRelease(&vsBlob);
		SafeRelease(&errorBlob);
		return;
	}

	hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &pVertexShader);
	if(FAILED(hr))
	{
		SafeRelease(&vsBlob);
		SafeRelease(&psBlob);
		return;
	}

	hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pPixelShader);
	if(FAILED(hr))
	{
		SafeRelease(&vsBlob);
		SafeRelease(&psBlob);

		return;
	}

	// Create a POSITION variable that is 32 bits per rgb value and is per vertex
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	hr = pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);

	SafeRelease(&vsBlob);
	SafeRelease(&psBlob);

	if(FAILED(hr))
	{
		return;
	}


}

void InstancedRendererEngine2D::CreateBuffers()
{
	square = new SquareMesh(*pDevice);
	triangle = new TriangleMesh(*pDevice);

	// Create buffer description for vertices
	D3D11_BUFFER_DESC bd = {};

	// Re-use buffer description for the input layout like position.
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VertexInputData);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	HRESULT hr = pDevice->CreateBuffer(&bd, nullptr, &pConstantBuffer);

	if(FAILED(hr)) return;
}

void InstancedRendererEngine2D::RenderWavingGrid(int gridWidth, int gridHeight)
{
	int columns = gridWidth;
	int rows = gridHeight;

	float startRenderPos = 1.0f / columns;

	VertexInputData cbData;

	cbData.objectPosX = 0.0f;
	cbData.objectPosY = 0.0f;
	cbData.aspectRatio = aspectRatioX;
	cbData.size = (2 / startRenderPos) * startRenderPos * 0.95f; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over width, * single element == size per element
	cbData.time = totalTime;
	cbData.speed = 4.0f;

	for(size_t i = 0; i < columns; i++)
	{

		cbData.objectPosX = startRenderPos * i;
		cbData.indexesX = i;

		for(size_t j = 0; j < rows; j++)
		{
			cbData.objectPosY = startRenderPos * j;
			cbData.indexesY = j;

			pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
			pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer); // Actually pass the variables to the vertex shader

			pDeviceContext->DrawIndexed(6, 0, 0);
		}
	}
}