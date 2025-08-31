#include "InstancedRendererEngine2D.h"
#include "imgui.h"
#include <cmath>

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

    // Create instance buffer, containing per-ant state
    UINT instanceCount = 1000;
    instances.reserve(instanceCount);

    for(UINT i = 0; i < instanceCount; ++i)
    {
        // { RandomGenerator::Generate(-1.0f,1.0f), RandomGenerator::Generate(-1.0f,1.0f) }
        InstanceData instance;
        // Start at nest with tiny jitter to reduce initial overlap
        float jx = RandomGenerator::Generate(-0.005f, 0.005f);
        float jy = RandomGenerator::Generate(-0.005f, 0.005f);
        instance.posX = nestPos.x + jx;
        instance.posY = nestPos.y + jy;
        instance.directionX = 0;
        instance.directionY = 0;
        instance.goalX = nestPos.x;
        instance.goalY = nestPos.y;
        instance.laneOffset = RandomGenerator::Generate(-0.03f, 0.03f);
        instance.speedScale = RandomGenerator::Generate(0.85f, 1.15f);
        instance.holdTimer = 0.0f;
        instance.color = DirectX::XMFLOAT4(RandomGenerator::Generate(0.1f, 1.0f), RandomGenerator::Generate(0.1f, 1.0f), RandomGenerator::Generate(0.1f, 1.0f), 1.0f);
        instance.movementState = 0; // start heading to food
        instance.sourceIndex = -1;

        instances.emplace(instances.begin() + i, instance);
    }

    // ImGui wrapper
    imgui = new ImGuiRenderer();
    imgui->Init(windowHandle, pDevice, pDeviceContext);

    // Load settings (optional file)
    LoadSettings();

    // Start game & stage
    ResetGame();
    StartStage(1);

    CreateMeshes();
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

    // Define a darker sand-like background to reduce brightness.
    const float clearColor[4] = { 0.55f, 0.50f, 0.40f, 1.0f };

    pDeviceContext->ClearRenderTargetView(renderTargetView, clearColor); // Clear the back buffer.

    // Ensure alpha blending is active each frame (ImGui resets states)
    if (blendState)
    {
        float blendFactor[4] = { 0,0,0,0 };
        UINT  sampleMask = 0xFFFFFFFF;
        pDeviceContext->OMSetBlendState(blendState, blendFactor, sampleMask);
    }

	// Update simple game loop and render the ant line
	UpdateGame(deltaTime);
    RenderFlock(activeAnts);

    // Markers: nest + all foods (geometry only)
    RenderMarker(nestPos, 2.0f, 2);
    RenderFoodMarkers();
    // Rain overlay on food only
    if (slowActive) {
        RenderRainOverlay();
    }
    // Hover outline overlay on top of fills
    POINT mp; GetCursorPos(&mp); ScreenToClient(windowHandle, &mp);
    int hoverIdx = FindNearestFoodScreen(mp.x, mp.y, 24.0f);
    if(hoverIdx >= 0 && hoverIdx < (int)foodNodes.size())
    {
        float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
        const auto& node = foodNodes[hoverIdx];
        float ratio = node.amount / base;
        ratio = max(0.3f, min(ratio, 2.5f));
        float sizeXFill = 2.0f * ratio;
        float sizeYFill = 2.0f * ratio;
        int baseColor = (hoverIdx == activeFoodIndex) ? 3 : 1;
        // Pixel-based desired thickness per layer (inner->outer): 3px, 2px, 1px
        float px[3] = { 3.0f, 2.0f, 1.0f };
        for(int i=0;i<3;i++)
        {
            int level = 3 - i;
            // Convert pixel thickness to shader size units (top/bottom use Y scaling, left/right use X with aspect)
            float tY = (px[i] * (2.0f / (float)screenHeight)) / 0.05f;
            float tX = (px[i] * (2.0f / (float)screenWidth)) / (0.05f / aspectRatioX);
            // top
            RenderRectTL(node.pos, sizeXFill, tY, baseColor, level);
            // bottom
            Vector2D bl = node.pos; bl.y = node.pos.y - (sizeYFill - tY) * 0.05f;
            RenderRectTL(bl, sizeXFill, tY, baseColor, level);
            // left
            RenderRectTL(node.pos, tX, sizeYFill, baseColor, level);
            // right
            Vector2D rl = node.pos; rl.x = node.pos.x + (sizeXFill - tX) * 0.05f / aspectRatioX;
            RenderRectTL(rl, tX, sizeYFill, baseColor, level);
        }
    }

    // (Text labels disabled per request)
    // ImGui frame & UI
    if (imgui)
    {
        imgui->NewFrame((float)screenWidth, (float)screenHeight, deltaTime);
        RenderUI();
        imgui->Render();
    }

	// Present the back buffer to the screen.
	// The first parameter (1) enables V-Sync, locking the frame rate to the monitor's refresh rate.
	// Change to 0 to disable V-Sync.
	pSwapChain->Present(1, 0);
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
    if (blendState) { blendState->Release(); blendState = nullptr; }
    SafeRelease(&scissorRasterizer);
    if (imgui) { imgui->Shutdown(); delete imgui; imgui = nullptr; }
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
    if (blendState) { blendState->Release(); blendState = nullptr; }
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

    hr = pDevice->CreateBlendState(&bsDesc, &blendState);

    if(FAILED(hr)) throw std::runtime_error("Failed to create blend state");

    float blendFactor[4] = { 0,0,0,0 };
    UINT  sampleMask = 0xFFFFFFFF;

    pDeviceContext->OMSetBlendState(blendState, blendFactor, sampleMask);
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
        lastFPS = currentFPS;
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

void InstancedRendererEngine2D::CreateMeshes()
{
	square = new SquareMesh(*pDevice);
	triangle = new TriangleMesh(*pDevice);
}

void InstancedRendererEngine2D::CreateBuffers()
{
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

	// Create a simple instance buffer to satisfy any copy usage
	D3D11_BUFFER_DESC instanceVBDesc = {};
	instanceVBDesc.Usage = D3D11_USAGE_DEFAULT;
	instanceVBDesc.ByteWidth = sizeof(InstanceData) * instanceCount;
	instanceVBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instanceVBDesc.CPUAccessFlags = 0;
	instanceVBDesc.MiscFlags = 0;

    hr = pDevice->CreateBuffer(&instanceVBDesc, nullptr, &instanceBuffer);

    // Food hit counts buffer (uint per node)
    D3D11_BUFFER_DESC countDesc = {};
    countDesc.Usage = D3D11_USAGE_DEFAULT;
    countDesc.ByteWidth = sizeof(UINT) * MaxFoodNodes * HitBins;
    countDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    countDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    countDesc.StructureByteStride = sizeof(UINT);
    hr = pDevice->CreateBuffer(&countDesc, nullptr, &foodCountBuffer);
    if(FAILED(hr)) throw std::runtime_error("Failed to create foodCountBuffer");
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavdc = {};
    uavdc.Format = DXGI_FORMAT_UNKNOWN;
    uavdc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavdc.Buffer.FirstElement = 0;
    uavdc.Buffer.NumElements = MaxFoodNodes;
    hr = pDevice->CreateUnorderedAccessView(foodCountBuffer, &uavdc, &foodCountUAV);
    if(FAILED(hr)) throw std::runtime_error("Failed to create foodCountUAV");
    // Staging buffers (ring) for readback
    D3D11_BUFFER_DESC rb = countDesc; rb.Usage = D3D11_USAGE_STAGING; rb.BindFlags = 0; rb.CPUAccessFlags = D3D11_CPU_ACCESS_READ; rb.MiscFlags = 0;
    for(int i=0;i<3;i++) { hr = pDevice->CreateBuffer(&rb, nullptr, &foodCountReadback[i]); if(FAILED(hr)) throw std::runtime_error("Failed to create foodCountReadback"); }
    // Create event queries for non-blocking mapping
    D3D11_QUERY_DESC qd = {}; qd.Query = D3D11_QUERY_EVENT; qd.MiscFlags = 0;
    for(int i=0;i<3;i++) { hr = pDevice->CreateQuery(&qd, &foodQuery[i]); if(FAILED(hr)) throw std::runtime_error("Failed to create foodQuery"); }

    // Nest hit counts buffer
    hr = pDevice->CreateBuffer(&countDesc, nullptr, &nestCountBuffer);
    if(FAILED(hr)) throw std::runtime_error("Failed to create nestCountBuffer");
    hr = pDevice->CreateUnorderedAccessView(nestCountBuffer, &uavdc, &nestCountUAV);
    if(FAILED(hr)) throw std::runtime_error("Failed to create nestCountUAV");
    for(int i=0;i<3;i++) { hr = pDevice->CreateBuffer(&rb, nullptr, &nestCountReadback[i]); if(FAILED(hr)) throw std::runtime_error("Failed to create nestCountReadback"); }
    for(int i=0;i<3;i++) { hr = pDevice->CreateQuery(&qd, &nestQuery[i]); if(FAILED(hr)) throw std::runtime_error("Failed to create nestQuery"); }
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
    float speedMul = slowActive ? 0.65f : 1.0f;
    float stateMul = (gameState == GameState::Playing) ? 1.0f : 0.0f;
    cbData.speed = antSpeed * speedMul * stateMul;
    // Compute shader uses targetPos = food, previousTargetPos = nest
    cbData.previousTargetPosX = nestPos.x;
    cbData.previousTargetPosY = nestPos.y;
    Vector2D target = (activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size()) ? foodNodes[activeFoodIndex].pos : nestPos;
    cbData.targetPosX = target.x;
    cbData.targetPosY = target.y;
    cbData.orbitDistance = 0.02f; // unused for spacing now; kept for stopDistance tuning
    cbData.jitter = 0.00025f;
    cbData.activeFoodIndex = activeFoodIndex;

	flockTransitionTime += deltaTime;
	cbData.flockTransitionTime = (float)flockTransitionTime;
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
    ID3DBlob* colorVsBlob = nullptr;
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

    shaderFilePath = L"UIPanelVertexShader.cso"; // Create UI panel vertex shader
    if(!Utilities::CreateVertexShader(pDevice, hr, shaderFilePath, &uiVertexShader, nullptr))
    {
        return;
    };

    shaderFilePath = L"FlockVertexShader.cso";
    if(!Utilities::CreateVertexShader(pDevice, hr, shaderFilePath, &flockVertexShader, &flockVsBlob))
    {
        return;
    };

    shaderFilePath = L"ColorVertexShader.cso";
    if(!Utilities::CreateVertexShader(pDevice, hr, shaderFilePath, &colorVertexShader, &colorVsBlob))
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
    SafeRelease(&colorVsBlob);
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

    // Clear hit counts to zero each frame via UAV clears
    const UINT zeros4[4] = {0,0,0,0};
    pDeviceContext->ClearUnorderedAccessViewUint(foodCountUAV, zeros4);
    pDeviceContext->ClearUnorderedAccessViewUint(nestCountUAV, zeros4);

    ID3D11UnorderedAccessView* uavs[] = { unorderedAccessViewB, foodCountUAV, nestCountUAV };
    UINT initialCounts[] = { 0, 0, 0 };
    pDeviceContext->CSSetUnorderedAccessViews(0, 3, uavs, initialCounts);

	pDeviceContext->UpdateSubresource(buffer, 0, nullptr, &cbData, 0, 0);
	pDeviceContext->CSSetConstantBuffers(0, 1, &buffer);

	pDeviceContext->CSSetShader(computeShader, nullptr, 0);
	pDeviceContext->Dispatch((instanceCount + 255) / 256, 1, 1);

    pDeviceContext->CopyResource(instanceBuffer, computeBufferA);

    // Read back food counts and decrease amounts per node
    int w = readbackCursor;
    int r = (readbackCursor + 1) % 3;
    pDeviceContext->CopyResource(foodCountReadback[w], foodCountBuffer);
    pDeviceContext->End(foodQuery[w]);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (pDeviceContext->GetData(foodQuery[r], nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
        SUCCEEDED(pDeviceContext->Map(foodCountReadback[r], 0, D3D11_MAP_READ, 0, &mapped)))
    {
        UINT* counts = (UINT*)mapped.pData;
        size_t n = foodNodes.size();
        for (size_t i = 0; i < n && i < (size_t)MaxFoodNodes; ++i)
        {
            // Sum across bins
            UINT sum = 0;
            for(int b=0;b<HitBins;b++) sum += counts[i*HitBins + b];
            if (sum > 0)
            {
                float dec = (float)sum;
                foodNodes[i].amount = max(0.0f, foodNodes[i].amount - dec);
            }
        }
        pDeviceContext->Unmap(foodCountReadback[r], 0);
    }

    // Read back nest counts and add to score/stageScore
    pDeviceContext->CopyResource(nestCountReadback[w], nestCountBuffer);
    pDeviceContext->End(nestQuery[w]);
    D3D11_MAPPED_SUBRESOURCE mappedN = {};
    if (pDeviceContext->GetData(nestQuery[r], nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
        SUCCEEDED(pDeviceContext->Map(nestCountReadback[r], 0, D3D11_MAP_READ, 0, &mappedN)))
    {
        UINT* countsN = (UINT*)mappedN.pData;
        UINT totalHits = 0;
        size_t n = foodNodes.size();
        for (size_t i = 0; i < n && i < (size_t)MaxFoodNodes; ++i)
        {
            for(int b=0;b<HitBins;b++) totalHits += countsN[i*HitBins + b];
        }
        if (totalHits > 0)
        {
            // Combo handling
            if (sinceLastDeposit < 0.5) combo = min(5, combo + 1); else combo = 1;
            sinceLastDeposit = 0.0;
            int add = (int)totalHits * combo;
            score += add;
            stageScore += add;
        }
        pDeviceContext->Unmap(nestCountReadback[r], 0);
    }

	// Swap,  backbuffer style
	std::swap(computeBufferA, computeBufferB);
	std::swap(shaderResourceViewA, shaderResourceViewB);
	std::swap(unorderedAccessViewA, unorderedAccessViewB);

	// Unset the compute buffers so they can be used elsewhere
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    ID3D11UnorderedAccessView* nullUAVs[3] = { nullptr, nullptr, nullptr };
    pDeviceContext->CSSetShaderResources(0, 1, nullSRVs);
    pDeviceContext->CSSetUnorderedAccessViews(0, 3, nullUAVs, nullptr);
    pDeviceContext->CSSetShader(nullptr, nullptr, 0);

    readbackCursor = (readbackCursor + 1) % 3;
}

void InstancedRendererEngine2D::RenderMarker(const Vector2D& ndcPos, float size, int colorCode)
{
    if(!square || !colorVertexShader || !plainPixelShader) return;

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pDeviceContext->IASetInputLayout(pInputLayout);
    SetupVerticesAndShaders(stride, offset, 1, square->renderingData, colorVertexShader, plainPixelShader);

    VertexInputData cbData = {};
    cbData.aspectRatio = aspectRatioX;
    cbData.sizeX = size; // scales the base 0.05 quad
    cbData.sizeY = size;
    cbData.objectPosX = ndcPos.x;
    cbData.objectPosY = ndcPos.y;
    cbData.indexesX = colorCode; // used by shader to pick color

    pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    pDeviceContext->DrawIndexed(square->renderingData->indexCount, 0, 0);
}

void InstancedRendererEngine2D::RenderText(int xPos, int yPos, int fontSize, const wchar_t* text)
{
    if(!text || !font) return;

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

    size_t textLength = wcslen(text);
    FontCharDescription fontDesc;
    Vector2D textureSize = font->GetTextureSize();

    for(size_t i = 0; i < textLength; i++)
    {
        fontDesc = font->GetFontCharacter(text[i]);

        cbData.objectPos.x = xPos + (int)i * (int)cbData.size.x;

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

void InstancedRendererEngine2D::RenderHud()
{
    wchar_t line1[128];
    wchar_t line2[128];
    wchar_t line3[128];
    wchar_t line4[128];
    wchar_t line5[128];

    const wchar_t* modeText = L"Idle";
    if(mode == AntMode::ToFood) modeText = L"ToFood";
    else if(mode == AntMode::ToNest) modeText = L"ToNest";

    swprintf_s(line1, L"Score: %d  Combo: x%d", score, combo);
    float remaining = 0.0f;
    if(activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size()) remaining = foodNodes[activeFoodIndex].amount;
    swprintf_s(line2, L"Food: %.0f", remaining);
    swprintf_s(line3, L"Mode: %s", modeText);
    swprintf_s(line4, L"Stage: %d  Target: %d  Time: %02.0f", stage, stageTarget - stageScore, stageTimeLeft);
    swprintf_s(line5, L"Event: %s", slowActive ? L"Rain (slow)" : L"-");

    RenderText(50, 90, 28, line1);
    RenderText(50, 120, 28, line2);
    RenderText(50, 150, 28, line3);
    RenderText(50, 180, 28, line4);
    RenderText(50, 210, 28, line5);

    // Stage overlays are rendered in OnPaint with translucent panels.
}

void InstancedRendererEngine2D::RenderRectTL(const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode, int lightenLevel)
{
    if(!square || !colorVertexShader || !plainPixelShader) return;

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pDeviceContext->IASetInputLayout(pInputLayout);
    SetupVerticesAndShaders(stride, offset, 1, square->renderingData, colorVertexShader, plainPixelShader);

    VertexInputData cbData = {};
    cbData.aspectRatio = aspectRatioX;
    cbData.sizeX = sizeX;
    cbData.sizeY = sizeY;
    cbData.objectPosX = ndcTopLeft.x;
    cbData.objectPosY = ndcTopLeft.y;
    cbData.indexesX = colorCode;
    cbData.indexesY = lightenLevel;

    pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    pDeviceContext->DrawIndexed(square->renderingData->indexCount, 0, 0);
}

void InstancedRendererEngine2D::RenderMarkerWithMesh(Mesh* mesh, const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode, int lightenLevel)
{
    if(!mesh || !colorVertexShader || !plainPixelShader) return;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pDeviceContext->IASetInputLayout(pInputLayout);
    SetupVerticesAndShaders(stride, offset, 1, mesh, colorVertexShader, plainPixelShader);

    VertexInputData cbData = {};
    cbData.aspectRatio = aspectRatioX;
    cbData.sizeX = sizeX;
    cbData.sizeY = sizeY;
    cbData.objectPosX = ndcTopLeft.x;
    cbData.objectPosY = ndcTopLeft.y;
    cbData.indexesX = colorCode;
    cbData.indexesY = lightenLevel;

    pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    pDeviceContext->DrawIndexed(mesh->indexCount, 0, 0);
}

void InstancedRendererEngine2D::RenderUI()
{
    // Per-node amounts as overlay (no windows)
    if (ImGui::GetCurrentContext())
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
        for (size_t i = 0; i < foodNodes.size(); ++i)
        {
            const auto& node = foodNodes[i];
            float ratio = node.amount / base;
            ratio = max(0.3f, min(ratio, 2.5f));
            // Compute on-screen rect center matching drawn footprint
            float sx = (node.pos.x + 1.0f) * 0.5f * (float)screenWidth;
            float sy = (1.0f - node.pos.y) * 0.5f * (float)screenHeight;
            float widthPx  = (0.05f * 2.0f * ratio / aspectRatioX) * ((float)screenWidth * 0.5f);
            float heightPx = (0.05f * 2.0f * ratio)                * ((float)screenHeight * 0.5f);
            ImVec2 center(sx + widthPx * 0.5f, sy + heightPx * 0.5f);

            wchar_t wbuf[32]; swprintf_s(wbuf, L"%.0f", max(0.0f, node.amount));
            char buf[32]; wcstombs(buf, wbuf, sizeof(buf));
            ImVec2 textSize = ImGui::CalcTextSize(buf);
            ImVec2 pos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
            // Shadow
            dl->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0,0,0,180), buf);
            // Text (slightly tinted by active state)
            ImU32 col = (i == (size_t)activeFoodIndex) ? IM_COL32(255, 240, 160, 230) : IM_COL32(255,255,255,220);
            dl->AddText(pos, col, buf);
        }
    }

    // HUD window (ImGui)
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("HUD", nullptr, flags))
    {
        const char* modeText = (mode == AntMode::ToFood) ? "ToFood" : (mode == AntMode::ToNest ? "ToNest" : "Idle");
        float remaining = 0.0f;
        if(activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size()) remaining = foodNodes[activeFoodIndex].amount;
        ImGui::Text("FPS: %.0f", lastFPS);
        ImGui::Text("Score: %d  Combo: x%d", score, combo);
        ImGui::Text("Mode: %s", modeText);
        ImGui::Text("Stage: %d  Target Left: %d  Time: %02.0f", stage, max(0, stageTarget - stageScore), stageTimeLeft);
        ImGui::Text("Ants: %d / %d", activeAnts, maxAnts);
        ImGui::Text("Event: %s", slowActive ? "Rain (slow)" : "-");
    }
    ImGui::End();

    // Overlays
    if(gameState == GameState::StageClear)
    {
        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(600, 180), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.7f);
        if (ImGui::Begin("Stage Clear", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Stage Clear! Choose Upgrade:");
            if (ImGui::Button("Speed +15%", ImVec2(180, 40))) ApplyUpgrade(1);
            ImGui::SameLine();
            if (ImGui::Button("Spawn Rate +25%", ImVec2(180, 40))) ApplyUpgrade(2);
            ImGui::SameLine();
            if (ImGui::Button("Max Ants +128", ImVec2(180, 40))) ApplyUpgrade(3);
        }
        ImGui::End();
    }
    else if(gameState == GameState::GameOver)
    {
        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(540, 150), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.7f);
        if (ImGui::Begin("Game Over", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Game Over!");
            if (ImGui::Button("Restart (R)", ImVec2(160, 36))) { ResetGame(); StartStage(1); }
            ImGui::SameLine();
            if (ImGui::Button("Endless Mode", ImVec2(160, 36))) { ToggleEndless(true); AdvanceStage(); }
        }
        ImGui::End();
    }
}

void InstancedRendererEngine2D::RenderRainOverlay()
{
    if (!square || !waveVertexShader || !plainPixelShader) return;

    // Full-field translucent wave overlay. UI is drawn after this, so it remains unaffected.
    const int gridX = 160;
    const int gridY = 90;
    RenderWavingGrid(gridX, gridY);
}

void InstancedRendererEngine2D::LoadSettings()
{
    // Try multiple candidate paths so it works from exe directory or project root
    const char* candidates[] = {
        "settings.ini",
        "../settings.ini",
        "../../settings.ini"
    };
    std::ifstream f;
    for(const char* path : candidates)
    {
        f.open(path);
        if(f.is_open()) break;
    }
    if(f.is_open())
    {
        std::string line;
        auto trim = [](std::string &s){ size_t a=s.find_first_not_of(" \t\r\n"); size_t b=s.find_last_not_of(" \t\r\n"); if(a==std::string::npos){s.clear();} else { s=s.substr(a,b-a+1);} };
        while(std::getline(f, line))
        {
            if(line.empty() || line[0] == '#' || line[0] == ';' || (line.size() > 1 && line[0] == '/' && line[1] == '/')) continue;
            auto pos = line.find('='); if(pos == std::string::npos) continue;
            std::string key = line.substr(0,pos); std::string val = line.substr(pos+1);
            trim(key); trim(val);
        try{
            if(key == "initialAnts") { initialAnts = max(0, stoi(val)); }
            else if(key == "antsPerSecond") antsPerSecond = max(0.0f, stof(val));
            else if(key == "spawnDelaySec") spawnDelaySec = max(0.0, stod(val));
            else if(key == "defaultFoodAmount") defaultFoodAmount = max(0.0f, stof(val));
            else if(key == "minFoodSpacing") minFoodSpacing = max(0.0f, stof(val));
            else if(key == "initialSpeed") { initialSpeed = max(0.0f, stof(val)); antSpeed = initialSpeed; }
        } catch(...) {}
        }
        f.close();
    }
    // Enforce: initial ants and max ants are the same
    maxAnts = initialAnts;
}
void InstancedRendererEngine2D::ResetAnts()
{
    // Reset all ants to nest and set goals to current active food (or nest if none)
    double sendInterval = max(0.02, 1.0 / max(0.01, (double)antsPerSecond));
    for (int i = 0; i < (int)instances.size(); ++i)
    {
        InstanceData &it = instances[i];
        it.posX = nestPos.x;
        it.posY = nestPos.y;
        it.directionX = 0.0f;
        it.directionY = 0.0f;
        it.goalX = nestPos.x;
        it.goalY = nestPos.y;
        it.movementState = 1; // ToNest (idle at nest)
        it.holdTimer = (i < activeAnts) ? (float)(sendInterval * i) : 9999.0f;
    }
    if(pDeviceContext && computeBufferA && computeBufferB && instanceBuffer)
    {
        pDeviceContext->UpdateSubresource(computeBufferA, 0, nullptr, instances.data(), 0, 0);
        pDeviceContext->CopyResource(computeBufferB, computeBufferA);
        pDeviceContext->CopyResource(instanceBuffer, computeBufferA);
    }
    // Reset timers for leg tracking
    legElapsed = 0.0;
    travelTime = 0.0;
}

void InstancedRendererEngine2D::RenderPanel(const Vector2D& ndcCenter, float sizeX, float sizeY, int colorCode)
{
    if(!square || !colorVertexShader || !plainPixelShader) return;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pDeviceContext->IASetInputLayout(pInputLayout);
    SetupVerticesAndShaders(stride, offset, 1, square->renderingData, colorVertexShader, plainPixelShader);

    VertexInputData cbData = {};
    cbData.aspectRatio = aspectRatioX;
    cbData.sizeX = sizeX;
    cbData.sizeY = sizeY;
    cbData.objectPosX = ndcCenter.x;
    cbData.objectPosY = ndcCenter.y;
    cbData.indexesX = colorCode;

    pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cbData, 0, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    pDeviceContext->DrawIndexed(square->renderingData->indexCount, 0, 0);
}

void InstancedRendererEngine2D::RenderUIPanel(int x, int y, int width, int height, float r, float g, float b, float a)
{
    if(!square || !uiVertexShader || !plainPixelShader) return;

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pDeviceContext->IASetInputLayout(pInputLayout);
    SetupVerticesAndShaders(stride, offset, 1, square->renderingData, uiVertexShader, plainPixelShader);

    struct UIPanelCB { float sizeX, sizeY; float posX, posY; float screenW, screenH; float color[4]; };
    UIPanelCB cb;
    cb.sizeX = (float)width;
    cb.sizeY = (float)height;
    cb.posX = (float)x;
    cb.posY = (float)y;
    cb.screenW = (float)screenWidth;
    cb.screenH = (float)screenHeight;
    cb.color[0] = r; cb.color[1] = g; cb.color[2] = b; cb.color[3] = a;

    pDeviceContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    pDeviceContext->DrawIndexed(square->renderingData->indexCount, 0, 0);
}

void InstancedRendererEngine2D::ApplyUpgrade(int option)
{
    if(gameState != GameState::StageClear) return;
    switch(option)
    {
        case 1: // Speed
            antSpeed *= 1.15f;
            break;
        case 2: // Spawn rate
            antsPerSecond *= 1.25f;
            // Re-stagger departures at nest to avoid clumping after rate change
            pendingSpawns.clear();
            spawnAccumulator = 0.0;
            RebuildDepartureStagger();
            break;
        case 3: // Max ants
            maxAnts = min(2048, maxAnts + 128);
            break;
        default:
            break;
    }
    AdvanceStage();
}

void InstancedRendererEngine2D::RebuildDepartureStagger()
{
    double sendInterval = max(0.02, 1.0 / max(0.01, (double)antsPerSecond));
    int queued = 0;
    for (int i = 0; i < activeAnts && i < (int)instances.size(); ++i)
    {
        InstanceData &it = instances[i];
        // Only adjust ants that are at the nest (ToNest) to prevent mid-air changes
        if (it.movementState == 1)
        {
            it.holdTimer = (float)(sendInterval * queued);
            ++queued;
        }
    }
    if(pDeviceContext && computeBufferA && computeBufferB)
    {
        pDeviceContext->UpdateSubresource(computeBufferA, 0, nullptr, instances.data(), 0, 0);
        pDeviceContext->CopyResource(computeBufferB, computeBufferA);
    }
}

void InstancedRendererEngine2D::ToggleEndless(bool enabled)
{
    endlessMode = enabled;
}

void InstancedRendererEngine2D::SetFood(int x, int y, float amount)
{
    SpawnFoodAtScreen(x, y, amount);
    // Select the newly spawned node (last)
    SetActiveFoodByIndex((int)foodNodes.size() - 1);
}

void InstancedRendererEngine2D::SetNest(int x, int y)
{
    nestPos.x = (x/(float)screenWidth * 2.0f) - 1.0f;
    nestPos.y = 1.0f - (y/(float)screenHeight * 2.0f);
    Vector2D target = (activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size()) ? foodNodes[activeFoodIndex].pos : nestPos;
    travelTime = (antSpeed > 0.0f) ? (std::sqrt((target.x - nestPos.x)*(target.x - nestPos.x) + (target.y - nestPos.y)*(target.y - nestPos.y)) / antSpeed) : 0.0;
}

void InstancedRendererEngine2D::UpdateGame(double dt)
{
    // Update hazard motion
    if(hazard.active)
    {
        hazard.pos.x += hazard.vel.x * (float)dt;
        hazard.pos.y += hazard.vel.y * (float)dt;
        // Bounce on NDC edges
        if(hazard.pos.x > 1.0f - hazard.radius) { hazard.pos.x = 1.0f - hazard.radius; hazard.vel.x *= -1.0f; }
        if(hazard.pos.x < -1.0f + hazard.radius) { hazard.pos.x = -1.0f + hazard.radius; hazard.vel.x *= -1.0f; }
        if(hazard.pos.y > 1.0f - hazard.radius) { hazard.pos.y = 1.0f - hazard.radius; hazard.vel.y *= -1.0f; }
        if(hazard.pos.y < -1.0f + hazard.radius) { hazard.pos.y = -1.0f + hazard.radius; hazard.vel.y *= -1.0f; }
    }

    // Stage/game timers and random slow event
    if(gameState == GameState::GameOver)
    {
        return;
    }
    if(gameState == GameState::StageClear)
    {
        // Idle until player advances
        mode = AntMode::Idle;
        return;
    }
    stageTimeLeft = max(0.0, stageTimeLeft - dt);
    slowSinceLast += dt;
    if(!slowActive && slowSinceLast >= slowCooldown)
    {
        // Small chance per second to trigger slow
        if(RandomGenerator::Generate(0.0f, 1.0f) > 0.96f)
        {
            slowActive = true;
            slowTimeLeft = 5.0;
            slowSinceLast = 0.0;
        }
    }
    else if(slowActive)
    {
        slowTimeLeft -= dt;
        if(slowTimeLeft <= 0.0)
        {
            slowActive = false;
        }
    }

    // Spawn strictly when food is selected (ToFood state)
    if(mode == AntMode::ToFood && activeFoodIndex >= 0)
    {
        int capacity = maxAnts - (activeAnts + (int)pendingSpawns.size());
        // Only schedule one pending spawn at a time, using spawnDelaySec from settings
        if (capacity > 0 && pendingSpawns.empty())
        {
            double interval = max(0.01, spawnDelaySec);
            pendingSpawns.push_back(interval);
        }
    }
    // Tick pending spawns and activate
    if(!pendingSpawns.empty())
    {
        // Decrease all timers by dt, but only activate one ant per frame
        for (double &t : pendingSpawns) t -= dt;
        if(pendingSpawns.front() <= 0.0 && activeAnts < maxAnts)
        {
            pendingSpawns.pop_front();
            // Activate one ant at slot = activeAnts
            int slot = activeAnts;
            activeAnts++;
            // Initialize its state near the nest to avoid overlap
            float ang = RandomGenerator::Generate(0.0f, 6.28318f);
            float rad = RandomGenerator::Generate(0.005f, 0.025f);
            float offX = (rad * cosf(ang));
            float offY = (rad * sinf(ang));
            InstanceData init = {};
            init.posX = nestPos.x + offX;
            init.posY = nestPos.y + offY;
            init.directionX = 0.0f;
            init.directionY = 0.0f;
            init.goalX = nestPos.x;
            init.goalY = nestPos.y;
            init.laneOffset = RandomGenerator::Generate(-0.03f, 0.03f);
            init.speedScale = RandomGenerator::Generate(0.85f, 1.15f);
            init.color = DirectX::XMFLOAT4(RandomGenerator::Generate(0.1f, 1.0f), RandomGenerator::Generate(0.1f, 1.0f), RandomGenerator::Generate(0.1f, 1.0f), 1.0f);
            init.movementState = 1; // ToNest (idle at nest)
            init.sourceIndex = -1;
            init.holdTimer = (float)max(0.01, spawnDelaySec);

            // Update compute buffers A and B at the slot
            UINT offsetBytes = slot * sizeof(InstanceData);
            D3D11_BOX box = {};
            box.left = offsetBytes;
            box.right = offsetBytes + sizeof(InstanceData);
            box.top = 0; box.bottom = 1; box.front = 0; box.back = 1;
            pDeviceContext->UpdateSubresource(computeBufferA, 0, &box, &init, 0, 0);
            pDeviceContext->UpdateSubresource(computeBufferB, 0, &box, &init, 0, 0);
        }
    }

    if(mode == AntMode::Idle) return;

    legElapsed += dt;
    sinceLastDeposit += dt;

    // Scoring handled via nest hit counts in compute shader readback

    if(antSpeed <= 0.0f) return;

    // Remove travelTime-based leg switching; compute shader governs transitions

    // Stage transitions
    if(stageScore >= stageTarget)
    {
        // Stage clear: reset ants immediately
        ResetAnts();
        gameState = GameState::StageClear;
    }
    else if(stageTimeLeft <= 0.0)
    {
        if(endlessMode)
        {
            // In endless, auto-advance and escalate
            AdvanceStage();
        }
        else
        {
            gameState = GameState::GameOver;
        }
    }
}

void InstancedRendererEngine2D::ResetGame()
{
    score = 0;
    stageScore = 0;
    stage = 1;
    gameState = GameState::Playing;
    combo = 1;
    sinceLastDeposit = 9999.0;
    slowActive = false;
    slowSinceLast = 0.0;
    slowTimeLeft = 0.0;
    foodNodes.clear();
    activeFoodIndex = -1;
    activeAnts = 0;
    spawnAccumulator = 0.0;
}

void InstancedRendererEngine2D::StartStage(int number)
{
    stage = number;
    stageTarget = 150 + (stage - 1) * 120;
    stageTimeLeft = 60.0 + (stage - 1) * 10.0;
    antsPerSecond = 20.0f + stage * 10.0f;
    // Do not auto-increase maxAnts across stages; only upgrades change it
    stageScore = 0;
    combo = 1;
    sinceLastDeposit = 9999.0;
    slowActive = false;
    slowSinceLast = 0.0;
    slowTimeLeft = 0.0;

    foodNodes.clear();
    int count = min(3 + stage, 10);
    SpawnRandomFood(count);
    // Do not auto-select; wait for user selection
    activeFoodIndex = -1;
    mode = AntMode::Idle;

    // Initialize with configured number of ants (idle at nest)
    activeAnts = min(maxAnts, initialAnts);
    spawnAccumulator = 0.0;
    pendingSpawns.clear();

    // Reset ants fully
    ResetAnts();
    mode = (activeFoodIndex >= 0) ? AntMode::ToFood : AntMode::Idle;
}

void InstancedRendererEngine2D::AdvanceStage()
{
    gameState = GameState::Playing;
    StartStage(stage + 1);
}

void InstancedRendererEngine2D::SpawnRandomFood(int count)
{
    for(int i = 0; i < count; ++i)
    {
        // Try to place a node with non-overlap
        for(int tries = 0; tries < 100; ++tries)
        {
            float x = RandomGenerator::Generate(-0.9f, 0.9f);
            float y = RandomGenerator::Generate(-0.9f, 0.9f);
            Vector2D p{x,y};
            bool ok = true;
            for(const auto& n : foodNodes)
            {
                float dx = n.pos.x - p.x;
                float dy = n.pos.y - p.y;
                if((dx*dx + dy*dy) < (minFoodSpacing*minFoodSpacing)) { ok = false; break; }
            }
            // Also keep away from nest
            if(ok)
            {
                float dnx = nestPos.x - p.x;
                float dny = nestPos.y - p.y;
                if((dnx*dnx + dny*dny) < (minFoodSpacing*minFoodSpacing)) ok = false;
            }
            if(ok)
            {
                // Randomize amount per node by stage
                float minAmt = defaultFoodAmount * (0.8f + 0.1f * (float)stage);
                float maxAmt = defaultFoodAmount * (1.5f + 0.2f * (float)stage);
                float amt = RandomGenerator::Generate(minAmt, maxAmt);
                FoodNode n{p, amt};
                // 30% chance to be triangle
                n.isTriangle = (RandomGenerator::Generate(0.0f, 1.0f) < 0.3f);
                foodNodes.push_back(n);
                break;
            }
        }
    }
    // Do not auto-select here
}

void InstancedRendererEngine2D::SpawnFoodAtScreen(int x, int y, float amount)
{
    Vector2D p;
    p.x = (x/(float)screenWidth * 2.0f) - 1.0f;
    p.y = 1.0f - (y/(float)screenHeight * 2.0f);
    // keep non-overlap
    for(const auto& n : foodNodes)
    {
        float dx = n.pos.x - p.x;
        float dy = n.pos.y - p.y;
        if((dx*dx + dy*dy) < (minFoodSpacing*minFoodSpacing)) return; // too close, skip
    }
    // keep away from nest
    {
        float dnx = nestPos.x - p.x;
        float dny = nestPos.y - p.y;
        if((dnx*dnx + dny*dny) < (minFoodSpacing*minFoodSpacing)) return;
    }
    foodNodes.push_back(FoodNode{p, amount});
}

int InstancedRendererEngine2D::FindNearestFoodScreen(int x, int y, float maxPixelRadius) const
{
    if(foodNodes.empty()) return -1;
    // Pixel-space hit test: axis-aligned rectangle matching rendered marker (top-left anchored)
    int cx = x;
    int cy = y;
    float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
    int best = -1;
    float bestD2 = 1e12f;
    for(size_t i = 0; i < foodNodes.size(); ++i)
    {
        const auto& node = foodNodes[i];
        float sx = (node.pos.x + 1.0f) * 0.5f * (float)screenWidth;
        float sy = (1.0f - node.pos.y) * 0.5f * (float)screenHeight;
        float ratio = node.amount / base;
        ratio = max(0.3f, min(ratio, 2.5f));
        // width/height in pixels match shader math: base quad 0.05, size=2*ratio, aspect divides X
        float widthPx  = (0.05f * 2.0f * ratio / aspectRatioX) * ((float)screenWidth * 0.5f);
        float heightPx = (0.05f * 2.0f * ratio)                * ((float)screenHeight * 0.5f);
        if(cx >= sx && cx <= sx + widthPx && cy >= sy && cy <= sy + heightPx)
        {
            float dx = (sx + widthPx * 0.5f) - cx;
            float dy = (sy + heightPx * 0.5f) - cy;
            float d2 = dx*dx + dy*dy;
            if(d2 < bestD2) { bestD2 = d2; best = (int)i; }
        }
    }
    return best;
}

void InstancedRendererEngine2D::SetActiveFoodByIndex(int index)
{
    if(index < 0)
    {
        // Deselect: keep current journeys; ants at nest will stay put
        activeFoodIndex = -1;
        return;
    }
    if(index >= (int)foodNodes.size()) return;
    activeFoodIndex = index;
    mode = AntMode::ToFood;
    legElapsed = 0.0;
    Vector2D target = foodNodes[activeFoodIndex].pos;
    travelTime = (antSpeed > 0.0f) ? (std::sqrt((target.x - nestPos.x)*(target.x - nestPos.x) + (target.y - nestPos.y)*(target.y - nestPos.y)) / antSpeed) : 0.0;
    int fx = (int)((target.x + 1.0f) * 0.5f * screenWidth);
    int fy = (int)((1.0f - target.y) * 0.5f * screenHeight);
    SetFlockTarget(fx, fy);
}
void InstancedRendererEngine2D::RenderFoodMarkers()
{
    float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
    for(size_t i = 0; i < foodNodes.size(); ++i)
    {
        const auto& node = foodNodes[i];
        float ratio = node.amount / base;
        ratio = max(0.3f, min(ratio, 2.5f));
        int color = (static_cast<int>(i) == activeFoodIndex) ? 3 : 1;
        if (node.isTriangle && triangle && triangle->renderingData)
        {
            // TriangleMesh is centered horizontally; adjust if needed by using TL anchor directly
            RenderMarkerWithMesh(triangle->renderingData, node.pos, 2.0f * ratio, 2.0f * ratio, color);
        }
        else
        {
            RenderMarker(node.pos, 2.0f * ratio, color);
        }
    }
}

void InstancedRendererEngine2D::RenderFoodLabels()
{
    float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
    for(const auto& node : foodNodes)
    {
        int sx = (int)((node.pos.x + 1.0f) * 0.5f * (float)screenWidth);
        int sy = (int)((1.0f - node.pos.y) * 0.5f * (float)screenHeight);
        wchar_t buf[64];
        swprintf_s(buf, L"%.0f", max(0.0f, node.amount));
        int len = (int)wcslen(buf);
        int fontSize = 22;
        int startX = sx - (len * fontSize) / 2;
        RenderText(startX, sy - fontSize/2, fontSize, buf);
    }
}
