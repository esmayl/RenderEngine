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
	width = rc.right - rc.left;
	height = rc.bottom - rc.top;
	
	//blocks = Utilities::CreateBlocks(width, height,width / blockWidth, height / blockHeight);

	SetupViewport(width, height);

	CreateShaders();
	CreateFonts(pDevice);
	CreateInstanceList();
	CreateBuffers();
}

void InstancedRendererEngine2D::SetupViewport(UINT newWidth, UINT newHeight)
{
	// Set up the viewport
	D3D11_VIEWPORT vp = {};

	vp.Width = (FLOAT)newWidth;
	vp.Height = (FLOAT)newHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	pDeviceContext->RSSetViewports(1, &vp);

	// Used for correcting triangles to their original shape and ignoring the aspect ratio of the viewport
	aspectRatioX = (newHeight > 0) ? (FLOAT)newHeight / (FLOAT)newWidth : 1.0f;
}

void InstancedRendererEngine2D::InitRenderBufferAndTargetView(HRESULT& hr)
{
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if(FAILED(hr)) return;

	hr = pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView);
	if(FAILED(hr)) return;

	pDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

	// Setup alpha blending
	D3D11_BLEND_DESC bsDesc = {};
	bsDesc.AlphaToCoverageEnable = FALSE;
	bsDesc.IndependentBlendEnable = FALSE;
	bsDesc.RenderTarget[0].BlendEnable = TRUE;
	bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bsDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bsDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bsDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bsDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bsDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	ID3D11BlendState* pBlendState = nullptr;
	hr = pDevice->CreateBlendState(&bsDesc, &pBlendState);
	if(FAILED(hr)) throw std::runtime_error("Failed to create blend state");

	float blendFactor[4] = { 0,0,0,0 };
	UINT  sampleMask = 0xFFFFFFFF;
	pDeviceContext->OMSetBlendState(pBlendState, blendFactor, sampleMask);
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

	//RenderWavingGrid(300,150);
	RenderFlock(1000);
	RenderFpsText(50, 50, 32);
	// Present the back buffer to the screen.
	// The first parameter (1) enables V-Sync, locking the frame rate to the monitor's refresh rate.
	// Change to 0 to disable V-Sync.
	pSwapChain->Present(0, 0);
}

void InstancedRendererEngine2D::OnResize(int newWidth, int newHeight)
{
	pDeviceContext->OMSetRenderTargets(0, 0, 0);
	SafeRelease(&renderTargetView);

	pSwapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);

	width = newWidth;
	height = newHeight;

	HRESULT hr = S_OK;
	InitRenderBufferAndTargetView(hr);
	
	if(FAILED(hr)) return;

	SetupViewport(newWidth, newHeight);
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
		swprintf_s(fpsText, L"FPS:%03.0f", currentFPS); // Update the global text buffer

		// Reset for the next second
		timeSinceFPSUpdate = 0.0;
		framesSinceFPSUpdate = 0;
	}
}

void InstancedRendererEngine2D::OnShutdown()
{
	delete square;
	delete triangle;
}


void InstancedRendererEngine2D::CreateShaders()
{
	HRESULT hr = S_FALSE;

	const wchar_t* shaderFilePath = L"SquareWaveVertexShader.hlsl"; // Create wave vertex shader
	if(!CreateVertexShader(hr, shaderFilePath, &waveVertexShader)){
		return;
	};

	shaderFilePath = L"TextVertexShader.hlsl"; // Create text vertex shader
	if(!CreateVertexShader(hr, shaderFilePath, &textVertexShader))
	{
		return;
	};

	shaderFilePath = L"FlockVertexShader.hlsl";
	if(!CreateVertexShader(hr, shaderFilePath, &flockVertexShader))
	{
		return;
	};

	shaderFilePath = L"PlainPixelShader.hlsl"; // Path to your HLSL file
	if(!CreatePixelShader(hr, shaderFilePath, &plainPixelShader))
	{
		return;
	};

	shaderFilePath = L"TextPixelShader.hlsl"; // Path to your HLSL file
	if(!CreatePixelShader(hr, shaderFilePath, &textPixelShader))
	{
		return;
	};


	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	// For now just use the TextVertexShader to make the input the same for all shaders, just used to fill vsBlob
	hr = D3DCompileFromFile(L"TextVertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
	if(FAILED(hr))
	{
		SafeRelease(&errorBlob);
		return;
	}

	// Create a POSITION variable that is 32 bits per rgb value and is per vertex
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		D3D11_INPUT_ELEMENT_DESC{"INSTANCEPOS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	hr = pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);

	if(FAILED(hr))
	{
		SafeRelease(&errorBlob);
		return;
	}

	// For now just use the TextVertexShader to make the input the same for all shaders, just used to fill vsBlob
	hr = D3DCompileFromFile(L"FlockVertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
	if(FAILED(hr))
	{
		SafeRelease(&errorBlob);
		return;
	}

	// This layout ONLY describes what the flock shader needs.
	D3D11_INPUT_ELEMENT_DESC flockLayout[] = {
		// Data from the Vertex Buffer (Slot 0)
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// Data from the Instance Buffer (Slot 1)
		{ "INSTANCEPOS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
	};

	hr = pDevice->CreateInputLayout(
		flockLayout,
		ARRAYSIZE(flockLayout),
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		&flockInputLayout // Create the dedicated layout
	);

	if(FAILED(hr))
	{
		SafeRelease(&vsBlob);
		SafeRelease(&errorBlob);
		return;
	}

	SafeRelease(&vsBlob);
	SafeRelease(&errorBlob);
}

bool InstancedRendererEngine2D::CreateVertexShader(HRESULT& hr, const wchar_t* vsFilePath, ID3D11VertexShader** vertexShader)
{
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	hr = D3DCompileFromFile(vsFilePath, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
	if(FAILED(hr))
	{
		SafeRelease(&errorBlob);
		return false;
	}
	hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader);
	if(FAILED(hr))
	{
		SafeRelease(&vsBlob);
		return false;
	}

	SafeRelease(&vsBlob);
	SafeRelease(&errorBlob);

	return true;
}

bool InstancedRendererEngine2D::CreatePixelShader(HRESULT& hr, const wchar_t* psFilePath, ID3D11PixelShader** pixelShader)
{
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	hr = D3DCompileFromFile(psFilePath, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
	if(FAILED(hr))
	{
		SafeRelease(&errorBlob);
		return false;
	}
	hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader);
	if(FAILED(hr))
	{
		SafeRelease(&psBlob);
		return false;
	}

	SafeRelease(&psBlob);
	SafeRelease(&errorBlob);

	return true;
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
	hr = pDevice->CreateBuffer(&bd, nullptr, &flockConstantBuffer);

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // Use linear filtering for smooth scaling
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = pDevice->CreateSamplerState(&samplerDesc, &textureSamplerState);

	if(FAILED(hr)) return;

	// Create instance buffer based on the instances vector
	D3D11_BUFFER_DESC instanceBufferDesc = {};
	instanceBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	instanceBufferDesc.ByteWidth = sizeof(InstanceData) * instances.size();
	instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // Instance buffers are bound as vertex buffers
	instanceBufferDesc.CPUAccessFlags = 0; // No flags set

	D3D11_SUBRESOURCE_DATA instanceData = {};
	instanceData.pSysMem = instances.data(); // Assing the created data to the subresource

	hr = pDevice->CreateBuffer(&instanceBufferDesc, &instanceData, &instanceBuffer);

	if(FAILED(hr)) return;
}

void InstancedRendererEngine2D::CreateFonts(ID3D11Device* device)
{
	font = new Font();
	font->LoadFonts("testFont.fnt");
	font->LoadTexture(device);
}

void InstancedRendererEngine2D::CreateInstanceList()
{
	UINT instanceCount = 1000;

	instances.reserve(instanceCount);

	for(int i = 0; i < instanceCount; ++i)
	{
		instances.push_back({ RandomGenerator::Generate(-1.0f,1.0f), RandomGenerator::Generate(-1.0f,1.0f) });
	}
}

void InstancedRendererEngine2D::RenderWavingGrid(int gridWidth, int gridHeight)
{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	pDeviceContext->IASetVertexBuffers(0, 1, &square->renderingData->vertexBuffer, &stride, &offset);
	pDeviceContext->IASetIndexBuffer(square->renderingData->indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Define what primitive we should draw with the vertex and indices

	pDeviceContext->VSSetShader(waveVertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(plainPixelShader, nullptr, 0);

	int columns = gridWidth;
	int rows = gridHeight;

	float startRenderPosX = 2.0f / columns;
	float startRenderPosY = 2.0f / rows;

	float factorX = startRenderPosX * 40.0f; // 40.0 is the factor based on the vertex pos 0.05f
	float factorY = startRenderPosY * 40.0f;
	VertexInputData cbData;

	cbData.aspectRatio = aspectRatioX;
	cbData.sizeX = factorX * aspectRatioX; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over width, * single element == size per element
	cbData.sizeY = factorY; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over width, * single element == size per element
	cbData.time = totalTime;
	cbData.speed = 4.0f;

	cbData.gridX = gridWidth;
	cbData.gridY = gridHeight;

	pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
	pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer); // Actually pass the variables to the vertex shader
	pDeviceContext->DrawIndexedInstanced(square->renderingData->indexCount,gridWidth * gridHeight,0,0,0);
}

void InstancedRendererEngine2D::RenderFlock(int instanceCount)
{
	UINT strides[] = { sizeof(Vertex), sizeof(InstanceData) };
	UINT offsets[] = { 0, 0 };

	ID3D11Buffer* buffers[] = { square->renderingData->vertexBuffer, instanceBuffer };

	// Setting the input layout to pass in instance position created on the cpu
	pDeviceContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	pDeviceContext->IASetIndexBuffer(square->renderingData->indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	pDeviceContext->IASetInputLayout(flockInputLayout); // Setup input variables for the vertex shader like the vertex position

	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Define what primitive we should draw with the vertex and indices

	pDeviceContext->VSSetShader(flockVertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(plainPixelShader, nullptr, 0);

	VertexInputData cbData;

	cbData.aspectRatio = aspectRatioX;
	cbData.time = totalTime;
	cbData.speed = 4.0f;

	pDeviceContext->UpdateSubresource(flockConstantBuffer, 0, nullptr, &cbData, 0, 0);
	pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer); // Actually pass the variables to the vertex shader
	pDeviceContext->DrawIndexedInstanced(square->renderingData->indexCount, instanceCount, 0, 0, 0);
}

void InstancedRendererEngine2D::RenderFpsText(int xPos, int yPos, int fontSize)
{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	pDeviceContext->IASetVertexBuffers(0, 1, &square->renderingData->vertexBuffer, &stride, &offset);
	pDeviceContext->IASetIndexBuffer(square->renderingData->indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	pDeviceContext->IASetInputLayout(pInputLayout); // Setup input variables for the vertex shader like the vertex position

	ID3D11ShaderResourceView* pTexture = font->GetTexture();
	pDeviceContext->PSSetShaderResources(0, 1, &pTexture); // Binds to register t0 in HLSL
	pDeviceContext->PSSetSamplers(0, 1, &textureSamplerState); // Binds to s0

	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Define what primitive we should draw with the vertex and indices

	pDeviceContext->VSSetShader(textVertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(textPixelShader, nullptr, 0);

	TextInputData cbData;
	cbData.screenSize.x = width;
	cbData.screenSize.y = height;
	cbData.objectPos.y = yPos;

	Vector4D fontPadding = font->GetPadding();
	cbData.size.x = fontSize;
	cbData.size.y = fontSize;

	size_t fpsTextLength = wcslen(fpsText);
	FontCharDescription fontDesc;
	Vector2D textureSize = font->GetTextureSize();

	for(size_t i = 0; i < fpsTextLength; i++)
	{
		fontDesc = font->GetFontCharacter(fpsText[i]);

		cbData.objectPos.x = xPos + i * cbData.size.x;

		cbData.uvOffset.x = ((float)fontDesc.x - fontPadding.x) / textureSize.x;
		cbData.uvOffset.y = ((float)fontDesc.y - fontPadding.y) / textureSize.y;

		float u1 = (fontDesc.x + fontPadding.z + fontDesc.width) / textureSize.x;
		float v1 = (fontDesc.y + fontPadding.w + fontDesc.height) / textureSize.y;

		cbData.uvScale.x = u1 - cbData.uvOffset.x;
		cbData.uvScale.y = v1 - cbData.uvOffset.y;


		pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
		pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer); // Actually pass the variables to the vertex shader

		pDeviceContext->DrawIndexed(square->renderingData->indexCount, 0, 0);
	}
}

