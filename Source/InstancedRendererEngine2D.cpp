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
	screenWidth = rc.right - rc.left;
	screenHeight = rc.bottom - rc.top;
	
	SetupViewport(screenWidth, screenHeight);

	LoadShaders();

	// Load the font from fnt file and load the corrosponding texture into Font variable
	font = new Font();
	font->LoadFonts("testFont.fnt");
	font->LoadTexture(pDevice);

	// Create instance buffer, containing position per instance , starts randomized in screen space
	UINT instanceCount = 1000;
	instances.reserve(instanceCount);

	for(UINT i = 0; i < instanceCount; ++i)
	{
		instances.push_back({ RandomGenerator::Generate(-1.0f,1.0f), RandomGenerator::Generate(-1.0f,1.0f) });
	}

	CreateBuffers();
}

void InstancedRendererEngine2D::OnPaint(HWND windowHandle)
{
	CountFps();

	auto currentTime = std::chrono::steady_clock::now();
	startTime = currentTime;
	totalTime += deltaTime;

	if(!pDeviceContext || !pSwapChain)
	{
		return;
	}

	// Define a color to clear the window to.
	const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // A nice blue

	pDeviceContext->ClearRenderTargetView(renderTargetView, clearColor); // Clear the back buffer.

	RenderWavingGrid(150,80);
	RenderFlock(256);
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
	SafeRelease(&backBuffer);

	pSwapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);

	screenWidth = newWidth;
	screenHeight = newHeight;

	HRESULT hr = S_OK;
	InitRenderBufferAndTargetView(hr);
	
	if(FAILED(hr)) return;

	SetupViewport(newWidth, newHeight);
}

void InstancedRendererEngine2D::OnShutdown()
{
	delete square;
	delete triangle;
	delete font;
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
	aspectRatioX = (newHeight > 0) ? (FLOAT)newWidth / (FLOAT)newHeight : 1.0f;
}

void InstancedRendererEngine2D::InitRenderBufferAndTargetView(HRESULT& hr)
{
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	if(FAILED(hr))  throw std::runtime_error("Failed to create backBuffer");

	hr = pDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
	if(FAILED(hr))  throw std::runtime_error("Failed to create renderTargetView");

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

void InstancedRendererEngine2D::CountFps()
{
	auto frameStartTime = std::chrono::steady_clock::now();

	auto currentTime = std::chrono::steady_clock::now();
	deltaTime = std::chrono::duration<double>(currentTime - startTime).count();

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

void InstancedRendererEngine2D::SetFlockTarget(int x, int y)
{
	previousFlockTarget = flockTarget;
	flockTarget.x = (x/(float)screenWidth * 2.0f) - 1.0f;
	flockTarget.y = 1.0f - (y/(float)screenHeight * 2.0f);
	flockFrozenTime = flockTransitionTime;
	flockTransitionTime = 0;
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

	if(FAILED(hr)) throw std::runtime_error("Failed to create textureSamplerState");
	UINT instanceCount = (UINT)instances.size();

	// Create instance buffer based on the instances vector
	D3D11_BUFFER_DESC instanceBufferDesc = {};
	instanceBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	instanceBufferDesc.ByteWidth = sizeof(InstanceData) * instanceCount;
	instanceBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	instanceBufferDesc.CPUAccessFlags = 0; // No flags set
	instanceBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	instanceBufferDesc.StructureByteStride = sizeof(InstanceData);

	hr = pDevice->CreateBuffer(&instanceBufferDesc, nullptr, &computeBufferA);
	hr = pDevice->CreateBuffer(&instanceBufferDesc, nullptr, &computeBufferB);
	
	if(FAILED(hr)) throw std::runtime_error("Failed to create computerBuffer A or B");

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
	srvd.Format = DXGI_FORMAT_UNKNOWN;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvd.Buffer.ElementOffset = 0;
	srvd.Buffer.NumElements = instanceCount;

	if(computeBufferA == nullptr) throw std::runtime_error("computerBufferA == null");
	if(computeBufferB == nullptr) throw std::runtime_error("computerBufferB == null");

	hr = pDevice->CreateShaderResourceView(computeBufferA, &srvd, &shaderResourceViewA);
	hr = pDevice->CreateShaderResourceView(computeBufferB, &srvd, &shaderResourceViewB);

	if(FAILED(hr)) throw std::runtime_error("Failed to create shaderResourceView A or B");

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
	uavd.Format = DXGI_FORMAT_UNKNOWN;
	uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavd.Buffer.FirstElement = 0;
	uavd.Buffer.NumElements = instanceCount;

	hr = pDevice->CreateUnorderedAccessView(computeBufferA, &uavd, &unorderedAccessViewA);
	hr = pDevice->CreateUnorderedAccessView(computeBufferB, &uavd, &unorderedAccessViewB);

	if(FAILED(hr)) throw std::runtime_error("Failed to create unorderedAccessView A or B");

	// Actually send the initial random positions to the compute shader buffers
	pDeviceContext->UpdateSubresource(computeBufferA,0,nullptr,instances.data(),0,0);
	pDeviceContext->CopyResource(computeBufferB,computeBufferA);
}

void InstancedRendererEngine2D::RenderWavingGrid(int gridWidth, int gridHeight)
{
	UINT stride[] = { sizeof(Vertex)};
	UINT offset[] = { 0 };

	SetupVerticesAndShaders(*stride, *offset, 1, square->renderingData, waveVertexShader, plainPixelShader);

	float startRenderPosX = 2.0f / gridWidth;
	float startRenderPosY = 2.0f / gridHeight;

	float factorX = startRenderPosX * 40.0f; // 40.0 is the factor based on the vertex pos 0.05f
	float factorY = startRenderPosY * 40.0f;
	VertexInputData cbData;

	cbData.aspectRatio = aspectRatioX;
	cbData.sizeX = factorX * aspectRatioX; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over width, * single element == size per element
	cbData.sizeY = factorY; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over width, * single element == size per element
	cbData.time = (float)totalTime;
	cbData.speed = 4.0f;

	cbData.gridX = gridWidth;
	cbData.gridY = gridHeight;

	PassInputDataAndRunInstanced(pConstantBuffer, cbData, *square->renderingData, gridWidth * gridHeight);
}

void InstancedRendererEngine2D::RenderFlock(int instanceCount)
{
	UINT strides[] = { sizeof(Vertex), sizeof(InstanceData) };
	UINT offsets[] = { 0, 0 };

	ID3D11Buffer* buffers[] = { square->renderingData->vertexBuffer, instanceBuffer };
	ID3D11ShaderResourceView* shaderResources[] = { shaderResourceViewA };

	VertexInputData cbData;

	cbData.aspectRatio = aspectRatioX;
	cbData.time = (float)totalTime;
	cbData.speed = 0.3f;
	cbData.previousTargetPosX = previousFlockTarget.x;
	cbData.previousTargetPosY = previousFlockTarget.y;
	cbData.targetPosX = flockTarget.x;
	cbData.targetPosY = flockTarget.y;
	cbData.orbitDistance = 0.2f;
	cbData.jitter = 0.00025f;

	flockTransitionTime += deltaTime;
	cbData.flockTransitionTime = flockTransitionTime;
	cbData.deltaTime = (float)deltaTime;

	RunComputeShader(flockConstantBuffer, cbData, instanceCount, flockComputeShader);

	pDeviceContext->VSSetShaderResources(0,1, shaderResources);

	// Setting the input layout to pass in instance position created on the cpu
	pDeviceContext->IASetInputLayout(flockInputLayout); // Setup input variables for the vertex shader like the vertex position

	SetupVerticesAndShaders(*strides, *offsets, 2, square->renderingData, flockVertexShader, plainPixelShader);
	PassInputDataAndRunInstanced(flockConstantBuffer, cbData, *square->renderingData, instanceCount);
}



void InstancedRendererEngine2D::RenderFpsText(int xPos, int yPos, int fontSize)
{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	ID3D11ShaderResourceView* pTexture = font->GetTexture();
	pDeviceContext->PSSetShaderResources(0, 1, &pTexture); // Binds to register t0 in HLSL
	pDeviceContext->PSSetSamplers(0, 1, &textureSamplerState); // Binds to s0
	pDeviceContext->IASetInputLayout(pInputLayout); // Setup input variables for the vertex shader like the vertex position
	
	SetupVerticesAndShaders(stride,offset,1, square->renderingData,textVertexShader,textPixelShader);

	TextInputData cbData;
	cbData.screenSize.x = (float)screenWidth;
	cbData.screenSize.y = (float)screenHeight;
	cbData.objectPos.y = (float)yPos;

	Vector4D fontPadding = font->GetPadding();
	cbData.size.x = (float)fontSize;
	cbData.size.y = (float)fontSize;

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


void InstancedRendererEngine2D::LoadShaders()
{
	HRESULT hr = S_FALSE;
	ID3DBlob* textVsBlob = nullptr;
	ID3DBlob* flockVsBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	const wchar_t* shaderFilePath = L"SquareWaveVertexShader.cso"; // Create wave vertex shader
	if(!Utilities::CreateVertexShader(pDevice, hr, shaderFilePath, &waveVertexShader, nullptr))
	{
		return;
	};

	shaderFilePath = L"TextVertexShader.cso"; // Create text vertex shader
	if(!Utilities::CreateVertexShader(pDevice, hr, shaderFilePath, &textVertexShader, &textVsBlob))
	{
		return;
	};

	shaderFilePath = L"FlockVertexShader.cso";
	if(!Utilities::CreateVertexShader(pDevice, hr, shaderFilePath, &flockVertexShader, &flockVsBlob))
	{
		return;
	};

	shaderFilePath = L"PlainPixelShader.cso"; // Path to your HLSL file
	if(!Utilities::CreatePixelShader(pDevice, hr, shaderFilePath, &plainPixelShader))
	{
		return;
	};

	shaderFilePath = L"TextPixelShader.cso"; // Path to your HLSL file
	if(!Utilities::CreatePixelShader(pDevice, hr, shaderFilePath, &textPixelShader))
	{
		return;
	};

	shaderFilePath = L"FlockComputeShader.cso"; // Path to your HLSL file
	if(!Utilities::CreateComputeShader(pDevice, hr, shaderFilePath, &flockComputeShader))
	{
		return;
	};

	// Used by the TextVertexShader and SquareWaveShader
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		D3D11_INPUT_ELEMENT_DESC{"INSTANCEPOS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	hr = pDevice->CreateInputLayout(layout,
									ARRAYSIZE(layout),
									textVsBlob->GetBufferPointer(),
									textVsBlob->GetBufferSize(),
									&pInputLayout);

	if(FAILED(hr))
	{
		SafeRelease(&errorBlob);
		return;
	}

	// This layout ONLY describes what the flock shader needs.
	D3D11_INPUT_ELEMENT_DESC flockLayout[] = {
		// Data from the Vertex Buffer (Slot 0)
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "INSTANCEPOS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
	};

	hr = pDevice->CreateInputLayout(
		flockLayout,
		ARRAYSIZE(flockLayout),
		flockVsBlob->GetBufferPointer(),
		flockVsBlob->GetBufferSize(),
		&flockInputLayout // Create the dedicated layout
	);

	SafeRelease(&textVsBlob);
	SafeRelease(&errorBlob);
}


void InstancedRendererEngine2D::SetupVerticesAndShaders(UINT& stride, UINT& offset, UINT bufferCount, Mesh* mesh, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader)
{
	pDeviceContext->IASetVertexBuffers(0, bufferCount, &mesh->vertexBuffer, &stride, &offset);
	pDeviceContext->IASetIndexBuffer(mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Define what primitive we should draw with the vertex and indices

	pDeviceContext->VSSetShader(vertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(pixelShader, nullptr, 0);
}

void InstancedRendererEngine2D::PassInputDataAndRunInstanced(ID3D11Buffer* buffer, VertexInputData& cbData, Mesh& mesh, int instanceCount)
{
	pDeviceContext->UpdateSubresource(buffer, 0, nullptr, &cbData, 0, 0);
	pDeviceContext->VSSetConstantBuffers(0, 1, &buffer); // Actually pass the variables to the vertex shader
	pDeviceContext->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);
}

void InstancedRendererEngine2D::RunComputeShader(ID3D11Buffer* buffer, VertexInputData& cbData, int instanceCount, ID3D11ComputeShader* computeShader)
{
	ID3D11ShaderResourceView* srvs[] = { shaderResourceViewA };
	pDeviceContext->CSSetShaderResources(0, 1, srvs);

	ID3D11UnorderedAccessView* uavs[] = { unorderedAccessViewB };
	UINT initialCounts[] = { 0 };
	pDeviceContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

	pDeviceContext->UpdateSubresource(buffer, 0, nullptr, &cbData, 0, 0);
	pDeviceContext->CSSetConstantBuffers(0, 1, &buffer);

	pDeviceContext->CSSetShader(computeShader, nullptr, 0);
	pDeviceContext->Dispatch((instanceCount + 255) / 256, 1, 1);

	pDeviceContext->CopyResource(instanceBuffer, computeBufferA);

	// Swap,  backbuffer style
	std::swap(computeBufferA, computeBufferB);
	std::swap(shaderResourceViewA, shaderResourceViewB);
	std::swap(unorderedAccessViewA, unorderedAccessViewB);

	// Unset the compute buffers so they can be used elsewhere
	ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
	ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
	pDeviceContext->CSSetShaderResources(0, 1, nullSRVs);
	pDeviceContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
	pDeviceContext->CSSetShader(nullptr, nullptr, 0);
}