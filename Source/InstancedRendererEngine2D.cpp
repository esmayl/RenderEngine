#include "InstancedRendererEngine2D.h"

#include "Button.h"
#include "UIPanelCB.h"

#include <cmath>
#include <cstdio>
#include <string>

// Lightweight UI helpers for consistent overlays
static inline void DrawRoundedBackground( ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImU32 bg, ImU32 border,
                                          float radius )
{
    // Subtle drop shadow
    dl->AddRectFilled( ImVec2( p1.x + 2, p1.y + 2 ), ImVec2( p2.x + 2, p2.y + 2 ), IM_COL32( 0, 0, 0, 110 ), radius );
    // Body
    dl->AddRectFilled( p1, p2, bg, radius );
    // Border
    dl->AddRect( p1, p2, border, radius );
}

// Re-implemented SafeRelease helper for COM pointers used in this file
template <class T> inline void SafeRelease( T** ppT )
{
    if ( ppT && *ppT )
    {
        ( *ppT )->Release();
        *ppT = nullptr;
    }
}

void InstancedRendererEngine2D::Init( HWND windowHandle, int blockWidth, int blockHeight )
{
    HRESULT hr = S_OK;
    startTime  = std::chrono::steady_clock::now();

    // Create the Device and Swap Chain
    DXGI_SWAP_CHAIN_DESC sd = {};

    sd.BufferCount       = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = windowHandle;
    sd.SampleDesc.Count  = 1;
    sd.Windowed          = TRUE;

    // Manually set all 'feature' levels, basically the directx versions. Also check project settings and hlsl setting
    // per file for compiling model etc..
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1,
    };
    UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    // Create the device, device context, and swap chain.
    hr = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, numFeatureLevels,
                                        D3D11_SDK_VERSION, &sd, pSwapChain.ReleaseAndGetAddressOf(),
                                        pDevice.ReleaseAndGetAddressOf(), nullptr,
                                        pDeviceContext.ReleaseAndGetAddressOf() );

    if ( FAILED( hr ) )
        return;

    InitRenderBufferAndTargetView( hr );
    if ( FAILED( hr ) )
        return;

    // Get the window's client area size to set the viewport.
    RECT rc;

    GetClientRect( windowHandle, &rc );
    screenWidth  = rc.right - rc.left;
    screenHeight = rc.bottom - rc.top;

    SetupViewport( screenWidth, screenHeight );

    LoadShaders();

    // Create instance buffer, containing per-ant state
    UINT instanceCount = 1000;
    instances.reserve( instanceCount );

    for ( UINT i = 0; i < instanceCount; ++i )
    {
        // { RandomGenerator::Generate(-1.0f,1.0f), RandomGenerator::Generate(-1.0f,1.0f) }
        InstanceData instance;
        // Start at nest with tiny jitter to reduce initial overlap
        float jx            = RandomGenerator::Generate( -0.005f, 0.005f );
        float jy            = RandomGenerator::Generate( -0.005f, 0.005f );
        instance.posX       = nestPos.x + jx;
        instance.posY       = nestPos.y + jy;
        instance.directionX = 0;
        instance.directionY = 0;
        instance.goalX      = nestPos.x;
        instance.goalY      = nestPos.y;
        instance.laneOffset = RandomGenerator::Generate( -0.03f, 0.03f );
        instance.speedScale = RandomGenerator::Generate( 0.85f, 1.15f );
        instance.holdTimer  = 0.0f;
        instance.color =
            DirectX::XMFLOAT4( RandomGenerator::Generate( 0.1f, 1.0f ), RandomGenerator::Generate( 0.1f, 1.0f ),
                               RandomGenerator::Generate( 0.1f, 1.0f ), 1.0f );
        instance.movementState = 0; // start heading to food
        instance.sourceIndex   = -1;

        instances.emplace( instances.begin() + i, instance );
    }

    // ImGui wrapper
    imgui = std::make_unique<ImGuiRenderer>();
    imgui->Init( windowHandle, pDevice.Get(), pDeviceContext.Get() );

    // Load settings (optional file)
    LoadSettings();

    // Start game & stage
    ResetGame();
    StartStage( 1 );

    CreateMeshes();
    CreateBuffers();
}

void InstancedRendererEngine2D::OnPaint( HWND windowHandle )
{
    CountFps();

    auto currentTime = std::chrono::steady_clock::now();
    startTime        = currentTime;
    totalTime += deltaTime;

    if ( !pDeviceContext || !pSwapChain )
    {
        return;
    }

    // Define a darker sand-like background to reduce brightness.
    const float clearColor[4] = { 0.55f, 0.50f, 0.40f, 1.0f };

    pDeviceContext->ClearRenderTargetView( renderTargetView.Get(), clearColor ); // Clear the back buffer.

    // Ensure alpha blending is active each frame (ImGui resets states)
    // if (blendState)
    // {
    //     float blendFactor[4] = { 0,0,0,0 };
    //     UINT  sampleMask = 0xFFFFFFFF;
    //     pDeviceContext->OMSetBlendState(blendState, blendFactor, sampleMask);
    // }

    // Update simple game loop and render the ant line
    UpdateGame( deltaTime );
    if ( antsEnabled )
    {
        RenderFlock( activeAnts );
    }

    // Markers: nest + all foods (geometry only)
    RenderMarker( nestPos, 2.0f, 2 );
    RenderFoodMarkers();
    // Rain overlay on food only
    if ( slowActive )
    {
        RenderRainOverlay();
    }
    // Hover outline overlay on top of fills
    POINT mp;
    GetCursorPos( &mp );
    ScreenToClient( windowHandle, &mp );
    int hoverIdx = FindNearestFoodScreen( mp.x, mp.y, 24.0f );
    if ( hoverIdx >= 0 && hoverIdx < (int)foodNodes.size() )
    {
        float base       = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
        const auto& node = foodNodes[hoverIdx];
        float ratio      = node.amount / base;
        ratio            = max( 0.3f, min( ratio, 2.5f ) );
        float sizeXFill  = 2.0f * ratio;
        float sizeYFill  = 2.0f * ratio;
        int baseColor    = ( hoverIdx == activeFoodIndex ) ? 3 : 1;
        // Pixel-based desired thickness per layer (inner->outer): 3px, 2px, 1px
        float px[3] = { 3.0f, 2.0f, 1.0f };
        for ( int i = 0; i < 3; i++ )
        {
            int level = 3 - i;
            // Convert pixel thickness to shader size units (top/bottom use Y scaling, left/right use X with aspect)
            float tY = ( px[i] * ( 2.0f / (float)screenHeight ) ) / 0.05f;
            float tX = ( px[i] * ( 2.0f / (float)screenWidth ) ) / ( 0.05f / aspectRatioX );
            // top
            RenderRectTL( node.pos, sizeXFill, tY, baseColor, level );
            // bottom
            Vector2D bl = node.pos;
            bl.y        = node.pos.y - ( sizeYFill - tY ) * 0.05f;
            RenderRectTL( bl, sizeXFill, tY, baseColor, level );
            // left
            RenderRectTL( node.pos, tX, sizeYFill, baseColor, level );
            // right
            Vector2D rl = node.pos;
            rl.x        = node.pos.x + ( sizeXFill - tX ) * 0.05f / aspectRatioX;
            RenderRectTL( rl, tX, sizeYFill, baseColor, level );
        }
    }

    // ImGui frame & UI
    if ( imgui )
    {
        imgui->NewFrame( (float)screenWidth, (float)screenHeight, deltaTime );
        // Prominent status text (score + time) centered at top
        RenderProminentStatus();
        // Compact event banner (top-right), not in HUD window
        RenderEventBanner();
        // Small top-left stats (Stage/Ants) on main screen
        RenderTopLeftStats();
        RenderUI();
        imgui->Render();
    }

    // Present the back buffer to the screen.
    // The first parameter (1) enables V-Sync, locking the frame rate to the monitor's refresh rate.
    // Change to 0 to disable V-Sync.
    pSwapChain->Present( 1, 0 );
}

void InstancedRendererEngine2D::OnResize( int newWidth, int newHeight )
{
    pDeviceContext->OMSetRenderTargets( 0, 0, 0 );
    renderTargetView.Reset();
    backBuffer.Reset();

    pSwapChain->ResizeBuffers( 0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0 );

    screenWidth  = newWidth;
    screenHeight = newHeight;

    HRESULT hr = S_OK;
    InitRenderBufferAndTargetView( hr );

    if ( FAILED( hr ) )
        return;

    SetupViewport( newWidth, newHeight );
}

void InstancedRendererEngine2D::OnShutdown()
{
    square.reset();
    triangle.reset();

    // if (blendState) { blendState->Release(); blendState = nullptr; }
    scissorRasterizer.Reset();
    if ( imgui )
    {
        imgui->Shutdown();
        imgui.reset();
        imgui = nullptr;
    }
}

void InstancedRendererEngine2D::SetupViewport( UINT newWidth, UINT newHeight )
{
    // Set up the viewport
    D3D11_VIEWPORT vp = {};

    vp.Width    = (FLOAT)newWidth;
    vp.Height   = (FLOAT)newHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    pDeviceContext->RSSetViewports( 1, &vp );

    // Used for correcting triangles to their original shape and ignoring the aspect ratio of the viewport
    aspectRatioX = ( newHeight > 0 ) ? (FLOAT)newWidth / (FLOAT)newHeight : 1.0f;
}

void InstancedRendererEngine2D::InitRenderBufferAndTargetView( HRESULT& hr )
{
    hr = pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ),
                                reinterpret_cast<void**>( backBuffer.ReleaseAndGetAddressOf() ) );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create backBuffer" );

    hr = pDevice->CreateRenderTargetView( backBuffer.Get(), nullptr, renderTargetView.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create renderTargetView" );

    {
        ID3D11RenderTargetView* rtv = renderTargetView.Get();
        pDeviceContext->OMSetRenderTargets( 1, &rtv, nullptr );
    }

    // Setup alpha blending
    // if (blendState) { blendState->Release(); blendState = nullptr; }
    D3D11_BLEND_DESC bsDesc                      = {};
    bsDesc.AlphaToCoverageEnable                 = FALSE;
    bsDesc.IndependentBlendEnable                = FALSE;
    bsDesc.RenderTarget[0].BlendEnable           = TRUE;
    bsDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    bsDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bsDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bsDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bsDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    bsDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bsDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    // hr = pDevice->CreateBlendState(&bsDesc, &blendState);

    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create blend state" );

    float blendFactor[4] = { 0, 0, 0, 0 };
    UINT sampleMask      = 0xFFFFFFFF;

    // pDeviceContext->OMSetBlendState(blendState, blendFactor, sampleMask);
}

void InstancedRendererEngine2D::CountFps()
{
    auto frameStartTime = std::chrono::steady_clock::now();

    auto currentTime = std::chrono::steady_clock::now();
    deltaTime        = std::chrono::duration<double>( currentTime - startTime ).count();

    timeSinceFPSUpdate += deltaTime;
    framesSinceFPSUpdate++;

    if ( timeSinceFPSUpdate >= 1.0 )
    {
        double currentFPS = framesSinceFPSUpdate / timeSinceFPSUpdate;
        lastFPS           = currentFPS;
        swprintf_s( fpsText, L"FPS:%03.0f", currentFPS ); // Update the global text buffer

        // Reset for the next second
        timeSinceFPSUpdate   = 0.0;
        framesSinceFPSUpdate = 0;
    }
}

void InstancedRendererEngine2D::SetFlockTarget( int x, int y )
{
    previousFlockTarget = flockTarget;
    flockTarget.x       = ( x / (float)screenWidth * 2.0f ) - 1.0f;
    flockTarget.y       = 1.0f - ( y / (float)screenHeight * 2.0f );
    flockFrozenTime     = flockTransitionTime;
    flockTransitionTime = 0;
}

void InstancedRendererEngine2D::CreateMeshes()
{
    square   = std::make_unique<SquareMesh>( *pDevice.Get() );
    triangle = std::make_unique<TriangleMesh>( *pDevice.Get() );
}

void InstancedRendererEngine2D::CreateBuffers()
{
    // Create buffer description for vertices
    D3D11_BUFFER_DESC bd = {};

    // Re-use buffer description for the input layout like position.
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( VertexInputData );
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT hr = pDevice->CreateBuffer( &bd, nullptr, pConstantBuffer.ReleaseAndGetAddressOf() );
    hr         = pDevice->CreateBuffer( &bd, nullptr, flockConstantBuffer.ReleaseAndGetAddressOf() );

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // Use linear filtering for smooth scaling
    samplerDesc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD             = 0;
    samplerDesc.MaxLOD             = D3D11_FLOAT32_MAX;

    UINT instanceCount = (UINT)instances.size();

    // Create instance buffer based on the instances vector
    D3D11_BUFFER_DESC instanceBufferDesc   = {};
    instanceBufferDesc.Usage               = D3D11_USAGE_DEFAULT;
    instanceBufferDesc.ByteWidth           = sizeof( InstanceData ) * instanceCount;
    instanceBufferDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    instanceBufferDesc.CPUAccessFlags      = 0; // No flags set
    instanceBufferDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    instanceBufferDesc.StructureByteStride = sizeof( InstanceData );

    hr = pDevice->CreateBuffer( &instanceBufferDesc, nullptr, computeBufferA.ReleaseAndGetAddressOf() );
    hr = pDevice->CreateBuffer( &instanceBufferDesc, nullptr, computeBufferB.ReleaseAndGetAddressOf() );

    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create computerBuffer A or B" );

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format                          = DXGI_FORMAT_UNKNOWN;
    srvd.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFER;
    srvd.Buffer.ElementOffset            = 0;
    srvd.Buffer.NumElements              = instanceCount;

    if ( computeBufferA == nullptr )
        throw std::runtime_error( "computerBufferA == null" );
    if ( computeBufferB == nullptr )
        throw std::runtime_error( "computerBufferB == null" );

    hr = pDevice->CreateShaderResourceView( computeBufferA.Get(), &srvd, shaderResourceViewA.ReleaseAndGetAddressOf() );
    hr = pDevice->CreateShaderResourceView( computeBufferB.Get(), &srvd, shaderResourceViewB.ReleaseAndGetAddressOf() );

    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create shaderResourceView A or B" );

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
    uavd.Format                           = DXGI_FORMAT_UNKNOWN;
    uavd.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
    uavd.Buffer.FirstElement              = 0;
    uavd.Buffer.NumElements               = instanceCount;

    hr = pDevice->CreateUnorderedAccessView( computeBufferA.Get(), &uavd,
                                             unorderedAccessViewA.ReleaseAndGetAddressOf() );
    hr = pDevice->CreateUnorderedAccessView( computeBufferB.Get(), &uavd,
                                             unorderedAccessViewB.ReleaseAndGetAddressOf() );

    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create unorderedAccessView A or B" );

    // Actually send the initial random positions to the compute shader buffers
    pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, nullptr, instances.data(), 0, 0 );
    pDeviceContext->CopyResource( computeBufferB.Get(), computeBufferA.Get() );

    // Create a simple instance buffer to satisfy any copy usage
    D3D11_BUFFER_DESC instanceVBDesc = {};
    instanceVBDesc.Usage             = D3D11_USAGE_DEFAULT;
    instanceVBDesc.ByteWidth         = sizeof( InstanceData ) * instanceCount;
    instanceVBDesc.BindFlags         = D3D11_BIND_VERTEX_BUFFER;
    instanceVBDesc.CPUAccessFlags    = 0;
    instanceVBDesc.MiscFlags         = 0;

    hr = pDevice->CreateBuffer( &instanceVBDesc, nullptr, instanceBuffer.ReleaseAndGetAddressOf() );

    // Food hit counts buffer (uint per node)
    D3D11_BUFFER_DESC countDesc   = {};
    countDesc.Usage               = D3D11_USAGE_DEFAULT;
    countDesc.ByteWidth           = sizeof( UINT ) * MaxFoodNodes * HitBins;
    countDesc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    countDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    countDesc.StructureByteStride = sizeof( UINT );
    hr = pDevice->CreateBuffer( &countDesc, nullptr, foodCountBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create foodCountBuffer" );
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavdc = {};
    uavdc.Format                           = DXGI_FORMAT_UNKNOWN;
    uavdc.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
    uavdc.Buffer.FirstElement              = 0;
    uavdc.Buffer.NumElements               = MaxFoodNodes;
    hr = pDevice->CreateUnorderedAccessView( foodCountBuffer.Get(), &uavdc, foodCountUAV.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create foodCountUAV" );
    // Staging buffers (ring) for readback
    D3D11_BUFFER_DESC rb = countDesc;
    rb.Usage             = D3D11_USAGE_STAGING;
    rb.BindFlags         = 0;
    rb.CPUAccessFlags    = D3D11_CPU_ACCESS_READ;
    rb.MiscFlags         = 0;
    for ( int i = 0; i < 3; i++ )
    {
        hr = pDevice->CreateBuffer( &rb, nullptr, foodCountReadback[i].ReleaseAndGetAddressOf() );
        if ( FAILED( hr ) )
            throw std::runtime_error( "Failed to create foodCountReadback" );
    }
    // Create event queries for non-blocking mapping
    D3D11_QUERY_DESC qd = {};
    qd.Query            = D3D11_QUERY_EVENT;
    qd.MiscFlags        = 0;
    for ( int i = 0; i < 3; i++ )
    {
        hr = pDevice->CreateQuery( &qd, &foodQuery[i] );
        if ( FAILED( hr ) )
            throw std::runtime_error( "Failed to create foodQuery" );
    }

    // Nest hit counts buffer
    hr = pDevice->CreateBuffer( &countDesc, nullptr, nestCountBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create nestCountBuffer" );
    hr = pDevice->CreateUnorderedAccessView( nestCountBuffer.Get(), &uavdc, nestCountUAV.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create nestCountUAV" );
    for ( int i = 0; i < 3; i++ )
    {
        hr = pDevice->CreateBuffer( &rb, nullptr, nestCountReadback[i].ReleaseAndGetAddressOf() );
        if ( FAILED( hr ) )
            throw std::runtime_error( "Failed to create nestCountReadback" );
    }
    for ( int i = 0; i < 3; i++ )
    {
        hr = pDevice->CreateQuery( &qd, &nestQuery[i] );
        if ( FAILED( hr ) )
            throw std::runtime_error( "Failed to create nestQuery" );
    }
}

void InstancedRendererEngine2D::RenderWavingGrid( int gridWidth, int gridHeight )
{
    UINT stride[] = { sizeof( Vertex ) };
    UINT offset[] = { 0 };

    SetupVerticesAndShaders( *stride, *offset, 1, square->renderingData, waveVertexShader.Get(), plainPixelShader.Get() );

    float startRenderPosX = 2.0f / gridWidth;
    float startRenderPosY = 2.0f / gridHeight;

    float factorX = startRenderPosX * 40.0f; // 40.0 is the factor based on the vertex pos 0.05f
    float factorY = startRenderPosY * 40.0f;
    VertexInputData cbData;

    cbData.aspectRatio = aspectRatioX;
    cbData.sizeX = factorX * aspectRatioX; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over
                                           // width, * single element == size per element
    cbData.sizeY = factorY; // -1 to 1 == 2 , 2 / by single element distance from 0 == total size over width, * single
                            // element == size per element
    cbData.time  = (float)totalTime;
    cbData.speed = 4.0f;

    cbData.gridX = gridWidth;
    cbData.gridY = gridHeight;

    PassInputDataAndRunInstanced( pConstantBuffer.Get(), cbData, *square->renderingData, gridWidth * gridHeight );
}

void InstancedRendererEngine2D::RenderFlock( int instanceCount )
{
    UINT strides[] = { sizeof( Vertex ), sizeof( InstanceData ) };
    UINT offsets[] = { 0, 0 };

    ID3D11Buffer* buffers[]                     = { square->renderingData->vertexBuffer, instanceBuffer.Get() };
    ID3D11ShaderResourceView* shaderResources[] = { shaderResourceViewA.Get() };

    VertexInputData cbData;

    cbData.aspectRatio = aspectRatioX;
    cbData.time        = (float)totalTime;
    float speedMul     = slowActive ? 0.65f : 1.0f;
    float stateMul     = ( gameState == GameState::Playing ) ? 1.0f : 0.0f;
    cbData.speed       = antSpeed * speedMul * stateMul;
    // Compute shader uses targetPos = food, previousTargetPos = nest
    cbData.previousTargetPosX = nestPos.x;
    cbData.previousTargetPosY = nestPos.y;
    Vector2D target =
        ( activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size() ) ? foodNodes[activeFoodIndex].pos : nestPos;
    cbData.targetPosX      = target.x;
    cbData.targetPosY      = target.y;
    cbData.orbitDistance   = 0.02f; // unused for spacing now; kept for stopDistance tuning
    cbData.jitter          = 0.00025f;
    cbData.activeFoodIndex = activeFoodIndex;

    flockTransitionTime += deltaTime;
    cbData.flockTransitionTime = (float)flockTransitionTime;
    cbData.deltaTime           = (float)deltaTime;

    RunComputeShader( flockConstantBuffer.Get(), cbData, instanceCount, flockComputeShader.Get() );

    // Bind VB + instance streams and shaders for instanced draw
    pDeviceContext->IASetInputLayout( flockInputLayout.Get() );

    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pDeviceContext->IASetVertexBuffers( 0, 1, &square->renderingData->vertexBuffer, &stride, &offset );

    pDeviceContext->IASetIndexBuffer( square->renderingData->indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
    pDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    {
        ID3D11ShaderResourceView* vsSrvs[] = { shaderResourceViewA.Get() };
        pDeviceContext->VSSetShaderResources( 0, 1, vsSrvs );
    }
    pDeviceContext->VSSetShader( flockVertexShader.Get(), nullptr, 0 );
    pDeviceContext->PSSetShader( plainPixelShader.Get(), nullptr, 0 );
    pDeviceContext->UpdateSubresource( flockConstantBuffer.Get(), 0, nullptr, &cbData, 0, 0 );
    {
        ID3D11Buffer* cb = flockConstantBuffer.Get();
        pDeviceContext->VSSetConstantBuffers( 0, 1, &cb );
    }
    pDeviceContext->DrawIndexedInstanced( square->renderingData->indexCount, instanceCount, 0, 0, 0 );
}

void InstancedRendererEngine2D::LoadShaders()
{
    HRESULT hr            = S_FALSE;
    ID3DBlob* flockVsBlob = nullptr;
    ID3DBlob* colorVsBlob = nullptr;
    ID3DBlob* errorBlob   = nullptr;

    const wchar_t* shaderFilePath = L"SquareWaveVertexShader.cso"; // Create wave vertex shader
    if ( !Utilities::CreateVertexShader( pDevice.Get(), hr, shaderFilePath, &waveVertexShader, nullptr ) )
    {
        return;
    };

    shaderFilePath = L"UIPanelVertexShader.cso"; // Create UI panel vertex shader
    if ( !Utilities::CreateVertexShader( pDevice.Get(), hr, shaderFilePath, &uiVertexShader, nullptr ) )
    {
        return;
    };

    shaderFilePath = L"FlockVertexShader.cso";
    if ( !Utilities::CreateVertexShader( pDevice.Get(), hr, shaderFilePath, &flockVertexShader, &flockVsBlob ) )
    {
        return;
    };

    shaderFilePath = L"ColorVertexShader.cso";
    if ( !Utilities::CreateVertexShader( pDevice.Get(), hr, shaderFilePath, &colorVertexShader, &colorVsBlob ) )
    {
        return;
    };

    shaderFilePath = L"PlainPixelShader.cso"; // Path to your HLSL file
    if ( !Utilities::CreatePixelShader( pDevice.Get(), hr, shaderFilePath, &plainPixelShader ) )
    {
        return;
    };

    shaderFilePath = L"FlockComputeShader.cso"; // Path to your HLSL file
    if ( !Utilities::CreateComputeShader( pDevice.Get(), hr, shaderFilePath, &flockComputeShader ) )
    {
        return;
    };

    // Input layout for colored quads (POSITION, TEXCOORD, optional instance stream slot 1 if needed)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        // No per-instance data for color VS, but keep slot reserved if needed later
    };

    // Create the color input layout using the Color VS signature
    hr = pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), colorVsBlob->GetBufferPointer(), colorVsBlob->GetBufferSize(), &pInputLayout );
    if ( FAILED( hr ) )
    {
        SafeRelease( &colorVsBlob );
        SafeRelease( &errorBlob );
        return;
    }

    // This layout ONLY describes what the flock shader needs.
    D3D11_INPUT_ELEMENT_DESC flockLayout[] = {
        // Data from the Vertex Buffer (Slot 0)
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "INSTANCEPOS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };

    hr = pDevice->CreateInputLayout( flockLayout, ARRAYSIZE( flockLayout ), flockVsBlob->GetBufferPointer(),
                                     flockVsBlob->GetBufferSize(),
                                     &flockInputLayout // Create the dedicated layout
    );

    SafeRelease( &flockVsBlob );
    SafeRelease( &colorVsBlob );
    SafeRelease( &errorBlob );
}

void InstancedRendererEngine2D::SetupVerticesAndShaders( UINT& stride, UINT& offset, UINT bufferCount, Mesh* mesh,
                                                         ID3D11VertexShader* vertexShader,
                                                         ID3D11PixelShader* pixelShader )
{
    pDeviceContext->IASetVertexBuffers( 0, bufferCount, &mesh->vertexBuffer, &stride, &offset );
    pDeviceContext->IASetIndexBuffer( mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0 );

    pDeviceContext->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST ); // Define what primitive we should draw with the vertex and indices

    pDeviceContext->VSSetShader( vertexShader, nullptr, 0 );
    pDeviceContext->PSSetShader( pixelShader, nullptr, 0 );
}

void InstancedRendererEngine2D::PassInputDataAndRunInstanced( ID3D11Buffer* buffer, VertexInputData& cbData, Mesh& mesh,
                                                              int instanceCount )
{
    pDeviceContext->UpdateSubresource( buffer, 0, nullptr, &cbData, 0, 0 );
    pDeviceContext->VSSetConstantBuffers( 0, 1, &buffer ); // Actually pass the variables to the vertex shader
    pDeviceContext->DrawIndexedInstanced( mesh.indexCount, instanceCount, 0, 0, 0 );
}

void InstancedRendererEngine2D::RunComputeShader( ID3D11Buffer* buffer, VertexInputData& cbData, int instanceCount,
                                                  ID3D11ComputeShader* computeShader )
{
    ID3D11ShaderResourceView* srvs[] = { shaderResourceViewA.Get() };
    pDeviceContext->CSSetShaderResources( 0, 1, srvs );

    // Clear hit counts to zero each frame via UAV clears
    const UINT zeros4[4] = { 0, 0, 0, 0 };
    pDeviceContext->ClearUnorderedAccessViewUint( foodCountUAV.Get(), zeros4 );
    pDeviceContext->ClearUnorderedAccessViewUint( nestCountUAV.Get(), zeros4 );

    ID3D11UnorderedAccessView* uavs[] = { unorderedAccessViewB.Get(), foodCountUAV.Get(), nestCountUAV.Get() };
    UINT initialCounts[]              = { 0, 0, 0 };
    pDeviceContext->CSSetUnorderedAccessViews( 0, 3, uavs, initialCounts );

    pDeviceContext->UpdateSubresource( buffer, 0, nullptr, &cbData, 0, 0 );
    pDeviceContext->CSSetConstantBuffers( 0, 1, &buffer );

    pDeviceContext->CSSetShader( computeShader, nullptr, 0 );
    pDeviceContext->Dispatch( ( instanceCount + 255 ) / 256, 1, 1 );

    pDeviceContext->CopyResource( instanceBuffer.Get(), computeBufferB.Get() );

    // Read back food counts and decrease amounts per node
    int w = readbackCursor;
    int r = ( readbackCursor + 1 ) % 3;
    pDeviceContext->CopyResource( foodCountReadback[w].Get(), foodCountBuffer.Get() );
    pDeviceContext->End( foodQuery[w].Get() );
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if ( pDeviceContext->GetData( foodQuery[r].Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK &&
         SUCCEEDED( pDeviceContext->Map( foodCountReadback[r].Get(), 0, D3D11_MAP_READ, 0, &mapped ) ) )
    {
        UINT* counts = (UINT*)mapped.pData;
        size_t n     = foodNodes.size();
        for ( size_t i = 0; i < n && i < (size_t)MaxFoodNodes; ++i )
        {
            // Sum across bins
            UINT sum = 0;
            for ( int b = 0; b < HitBins; b++ )
                sum += counts[i * HitBins + b];
            if ( sum > 0 )
            {
                float dec           = (float)sum;
                foodNodes[i].amount = max( 0.0f, foodNodes[i].amount - dec );
            }
        }
        pDeviceContext->Unmap( foodCountReadback[r].Get(), 0 );
    }

    // Read back nest counts and add to score/stageScore
    pDeviceContext->CopyResource( nestCountReadback[w].Get(), nestCountBuffer.Get() );
    pDeviceContext->End( nestQuery[w].Get() );
    D3D11_MAPPED_SUBRESOURCE mappedN = {};
    if ( pDeviceContext->GetData( nestQuery[r].Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK &&
         SUCCEEDED( pDeviceContext->Map( nestCountReadback[r].Get(), 0, D3D11_MAP_READ, 0, &mappedN ) ) )
    {
        UINT* countsN  = (UINT*)mappedN.pData;
        UINT totalHits = 0;
        size_t n       = foodNodes.size();
        for ( size_t i = 0; i < n && i < (size_t)MaxFoodNodes; ++i )
        {
            for ( int b = 0; b < HitBins; b++ )
                totalHits += countsN[i * HitBins + b];
        }
        if ( totalHits > 0 )
        {
            // Combo handling
            if ( sinceLastDeposit < 0.5 )
                combo = min( 5, combo + 1 );
            else
                combo = 1;
            sinceLastDeposit = 0.0;
            int add          = (int)totalHits * combo;
            score += add;
            stageScore += add;
        }
        pDeviceContext->Unmap( nestCountReadback[r].Get(), 0 );
    }

    // Swap,  backbuffer style
    std::swap( computeBufferA, computeBufferB );
    std::swap( shaderResourceViewA, shaderResourceViewB );
    std::swap( unorderedAccessViewA, unorderedAccessViewB );
    {
        ID3D11ShaderResourceView* nullSRVs[1]  = { nullptr };
        ID3D11UnorderedAccessView* nullUAVs[3] = { nullptr, nullptr, nullptr };
        UINT nullCounts[3]                     = { 0, 0, 0 };
        pDeviceContext->CSSetShaderResources( 0, 1, nullSRVs );
        pDeviceContext->CSSetUnorderedAccessViews( 0, 3, nullUAVs, nullCounts );
        pDeviceContext->CSSetShader( nullptr, nullptr, 0 );
    }

    // Unset the compute buffers so they can be used elsewhere
    ID3D11ShaderResourceView* nullSRVs[1]  = { nullptr };
    ID3D11UnorderedAccessView* nullUAVs[3] = { nullptr, nullptr, nullptr };
    pDeviceContext->CSSetShaderResources( 0, 1, nullSRVs );
    pDeviceContext->CSSetUnorderedAccessViews( 0, 3, nullUAVs, nullptr );
    pDeviceContext->CSSetShader( nullptr, nullptr, 0 );

    readbackCursor = ( readbackCursor + 1 ) % 3;
}

void InstancedRendererEngine2D::RenderMarker( const Vector2D& ndcPos, float size, int colorCode )
{
    if ( !square || !colorVertexShader || !plainPixelShader )
        return;

    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pDeviceContext->IASetInputLayout( pInputLayout.Get() );
    SetupVerticesAndShaders( stride, offset, 1, square->renderingData, colorVertexShader.Get(),
                             plainPixelShader.Get() );

    VertexInputData cbData = {};
    cbData.aspectRatio     = aspectRatioX;
    cbData.sizeX           = size; // scales the base 0.05 quad
    cbData.sizeY           = size;
    cbData.objectPosX      = ndcPos.x;
    cbData.objectPosY      = ndcPos.y;
    cbData.indexesX        = colorCode; // used by shader to pick color

    pDeviceContext->UpdateSubresource( pConstantBuffer.Get(), 0, nullptr, &cbData, 0, 0 );
    {
        ID3D11Buffer* cb = pConstantBuffer.Get();
        pDeviceContext->VSSetConstantBuffers( 0, 1, &cb );
    }
    pDeviceContext->DrawIndexed( square->renderingData->indexCount, 0, 0 );
}

void InstancedRendererEngine2D::RenderHud()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font   = ImGui::GetFont();

    const wchar_t* modeText = L"Idle";
    if ( mode == AntMode::ToFood )
        modeText = L"ToFood";
    else if ( mode == AntMode::ToNest )
        modeText = L"ToNest";

    wchar_t line1[128];
    wchar_t line2[128];
    wchar_t line3[128];
    wchar_t line4[128];
    // no event line here; events shown via banner

    swprintf_s( line1, L"Score: %d  Combo: x%d", score, combo );
    float remaining = 0.0f;
    if ( activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size() )
        remaining = foodNodes[activeFoodIndex].amount;
    swprintf_s( line2, L"Food: %.0f", remaining );
    swprintf_s( line3, L"Mode: %s", modeText );
    swprintf_s( line4, L"Stage: %d", stage );

    auto drawAt = [&]( int x, int y, float size, const wchar_t* wtext )
    {
        char buf[256];
        wcstombs( buf, wtext, sizeof( buf ) );
        dl->AddText( font, size, ImVec2( (float)x + 1, (float)y + 1 ), IM_COL32( 0, 0, 0, 180 ), buf );
        dl->AddText( font, size, ImVec2( (float)x, (float)y ), IM_COL32( 255, 255, 255, 230 ), buf );
    };

    drawAt( 50, 90, 28.0f, line1 );
    drawAt( 50, 120, 28.0f, line2 );
    drawAt( 50, 150, 28.0f, line3 );
    drawAt( 50, 180, 28.0f, line4 );
    // no events in HUD; banner handles it

    // Stage overlays are rendered in OnPaint with translucent panels.
}

void InstancedRendererEngine2D::RenderRectTL( const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode,
                                              int lightenLevel )
{
    if ( !square || !colorVertexShader || !plainPixelShader )
        return;

    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pDeviceContext->IASetInputLayout( pInputLayout.Get() );
    SetupVerticesAndShaders( stride, offset, 1, square->renderingData, colorVertexShader.Get(),
                             plainPixelShader.Get() );

    VertexInputData cbData = {};
    cbData.aspectRatio     = aspectRatioX;
    cbData.sizeX           = sizeX;
    cbData.sizeY           = sizeY;
    cbData.objectPosX      = ndcTopLeft.x;
    cbData.objectPosY      = ndcTopLeft.y;
    cbData.indexesX        = colorCode;
    cbData.indexesY        = lightenLevel;

    pDeviceContext->UpdateSubresource( pConstantBuffer.Get(), 0, nullptr, &cbData, 0, 0 );
    {
        ID3D11Buffer* cb = pConstantBuffer.Get();
        pDeviceContext->VSSetConstantBuffers( 0, 1, &cb );
    }
    pDeviceContext->DrawIndexed( square->renderingData->indexCount, 0, 0 );
}

void InstancedRendererEngine2D::RenderMarkerWithMesh( Mesh* mesh, const Vector2D& ndcTopLeft, float sizeX, float sizeY,
                                                      int colorCode, int lightenLevel )
{
    if ( !mesh || !mesh->vertexBuffer || !mesh->indexBuffer )
        return;
    if ( !colorVertexShader || !plainPixelShader || !pInputLayout )
        return;
    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pDeviceContext->IASetInputLayout( pInputLayout.Get() );
    SetupVerticesAndShaders( stride, offset, 1, mesh, colorVertexShader.Get(), plainPixelShader.Get() );

    VertexInputData cbData = {};
    cbData.aspectRatio     = aspectRatioX;
    cbData.sizeX           = sizeX;
    cbData.sizeY           = sizeY;
    cbData.objectPosX      = ndcTopLeft.x;
    cbData.objectPosY      = ndcTopLeft.y;
    cbData.indexesX        = colorCode;
    cbData.indexesY        = lightenLevel;

    pDeviceContext->UpdateSubresource( pConstantBuffer.Get(), 0, nullptr, &cbData, 0, 0 );
    {
        ID3D11Buffer* cb = pConstantBuffer.Get();
        pDeviceContext->VSSetConstantBuffers( 0, 1, &cb );
    }
    pDeviceContext->DrawIndexed( mesh->indexCount, 0, 0 );
}

void InstancedRendererEngine2D::RenderUI()
{
    // Per-node amounts as overlay (no windows)
    if ( ImGui::GetCurrentContext() )
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float base     = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
        for ( size_t i = 0; i < foodNodes.size(); ++i )
        {
            const auto& node = foodNodes[i];
            float ratio      = node.amount / base;
            ratio            = max( 0.3f, min( ratio, 2.5f ) );
            // Compute on-screen rect center matching drawn footprint
            float sx       = ( node.pos.x + 1.0f ) * 0.5f * (float)screenWidth;
            float sy       = ( 1.0f - node.pos.y ) * 0.5f * (float)screenHeight;
            float widthPx  = ( 0.05f * 2.0f * ratio / aspectRatioX ) * ( (float)screenWidth * 0.5f );
            float heightPx = ( 0.05f * 2.0f * ratio ) * ( (float)screenHeight * 0.5f );
            ImVec2 center( sx + widthPx * 0.5f, sy + heightPx * 0.5f );

            wchar_t wbuf[32];
            swprintf_s( wbuf, L"%.0f", max( 0.0f, node.amount ) );
            char buf[32];
            wcstombs( buf, wbuf, sizeof( buf ) );
            ImVec2 textSize = ImGui::CalcTextSize( buf );
            ImVec2 pos( center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f );
            // Shadow
            dl->AddText( ImVec2( pos.x + 1, pos.y + 1 ), IM_COL32( 0, 0, 0, 180 ), buf );
            // Text (slightly tinted by active state)
            ImU32 col =
                ( i == (size_t)activeFoodIndex ) ? IM_COL32( 255, 240, 160, 230 ) : IM_COL32( 255, 255, 255, 220 );
            dl->AddText( pos, col, buf );
        }
    }

    // HUD window (ImGui) — closable like a debug menu
    if ( showDebugHud )
    {
        ImGui::SetNextWindowPos( ImVec2( 20, 20 ), ImGuiCond_Once );
        ImGui::SetNextWindowBgAlpha( 0.6f );
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        if ( ImGui::Begin( "HUD", &showDebugHud, flags ) )
        {
            // Keep HUD concise; main screen handles gameplay stats
            ImGui::Text( "FPS: %.0f", lastFPS );
            ImGui::Separator();
            ImGui::Text( "Debug Controls:" );
            ImGui::BulletText( "F1 or H: toggle HUD" );
            ImGui::BulletText( "F: Frenzy (if off cooldown)" );
        }
        ImGui::End();
    }

    // Overlays (stylized panels instead of windows)
    if ( gameState == GameState::StageClear )
    {
        RenderStageClearOverlay();
    }
    else if ( gameState == GameState::GameOver )
    {
        RenderGameOverOverlay();
    }
}

void InstancedRendererEngine2D::RenderRainOverlay()
{
    if ( !square || !waveVertexShader || !plainPixelShader )
        return;

    // Full-field translucent wave overlay. UI is drawn after this, so it remains unaffected.
    const int gridX = 160;
    const int gridY = 90;
    RenderWavingGrid( gridX, gridY );
}

void InstancedRendererEngine2D::LoadSettings()
{
    // Try multiple candidate paths so it works from exe directory or project root
    const char* candidates[] = { "settings.ini", "../settings.ini", "../../settings.ini" };
    std::ifstream f;
    for ( const char* path : candidates )
    {
        f.open( path );
        if ( f.is_open() )
            break;
    }
    if ( f.is_open() )
    {
        std::string line;
        auto trim = []( std::string& s )
        {
            size_t a = s.find_first_not_of( " \t\r\n" );
            size_t b = s.find_last_not_of( " \t\r\n" );
            if ( a == std::string::npos )
            {
                s.clear();
            }
            else
            {
                s = s.substr( a, b - a + 1 );
            }
        };
        while ( std::getline( f, line ) )
        {
            if ( line.empty() || line[0] == '#' || line[0] == ';' ||
                 ( line.size() > 1 && line[0] == '/' && line[1] == '/' ) )
                continue;
            auto pos = line.find( '=' );
            if ( pos == std::string::npos )
                continue;
            std::string key = line.substr( 0, pos );
            std::string val = line.substr( pos + 1 );
            trim( key );
            trim( val );
            try
            {
                if ( key == "initialAnts" )
                {
                    initialAnts = max( 0, stoi( val ) );
                }
                else if ( key == "antsPerSecond" )
                    antsPerSecond = max( 0.0f, stof( val ) );
                else if ( key == "spawnDelaySec" )
                    spawnDelaySec = max( 0.0, stod( val ) );
                else if ( key == "defaultFoodAmount" )
                    defaultFoodAmount = max( 0.0f, stof( val ) );
                else if ( key == "minFoodSpacing" )
                    minFoodSpacing = max( 0.0f, stof( val ) );
                else if ( key == "initialSpeed" )
                {
                    initialSpeed = max( 0.0f, stof( val ) );
                    antSpeed     = initialSpeed;
                }
            }
            catch ( ... )
            {
            }
        }
        f.close();
    }
    // Enforce: initial ants and max ants are the same
    maxAnts = initialAnts;
}
void InstancedRendererEngine2D::ResetAnts()
{
    // Reset all ants to nest and set goals to current active food (or nest if none)
    double sendInterval = max( 0.02, 1.0 / max( 0.01, (double)antsPerSecond ) );

    for ( int i = 0; i < (int)instances.size(); ++i )
    {
        InstanceData& it = instances[i];
        it.posX          = nestPos.x;
        it.posY          = nestPos.y;
        it.directionX    = 0.0f;
        it.directionY    = 0.0f;
        it.goalX         = nestPos.x;
        it.goalY         = nestPos.y;
        it.movementState = 1; // ToNest (idle at nest)
        it.holdTimer     = ( i < activeAnts ) ? (float)( sendInterval * i ) : 9999.0f;
    }

    if ( pDeviceContext && computeBufferA && computeBufferB && instanceBuffer )
    {
        pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, nullptr, instances.data(), 0, 0 );
        pDeviceContext->CopyResource( computeBufferB.Get(), computeBufferA.Get() );
        pDeviceContext->CopyResource( instanceBuffer.Get(), computeBufferB.Get() );
    }
    // Reset timers for leg tracking
    legElapsed = 0.0;
    travelTime = 0.0;
}

void InstancedRendererEngine2D::RenderPanel( const Vector2D& ndcCenter, float sizeX, float sizeY, int colorCode )
{
    if ( !square || !colorVertexShader || !plainPixelShader )
        return;

    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pDeviceContext->IASetInputLayout( pInputLayout.Get() );
    SetupVerticesAndShaders( stride, offset, 1, square->renderingData, colorVertexShader.Get(),
                             plainPixelShader.Get() );

    VertexInputData cbData = {};
    cbData.aspectRatio     = aspectRatioX;
    cbData.sizeX           = sizeX;
    cbData.sizeY           = sizeY;
    cbData.objectPosX      = ndcCenter.x;
    cbData.objectPosY      = ndcCenter.y;
    cbData.indexesX        = colorCode;

    pDeviceContext->UpdateSubresource( pConstantBuffer.Get(), 0, nullptr, &cbData, 0, 0 );

    {
        ID3D11Buffer* cb = pConstantBuffer.Get();
        pDeviceContext->VSSetConstantBuffers( 0, 1, &cb );
    }

    pDeviceContext->DrawIndexed( square->renderingData->indexCount, 0, 0 );
}

void InstancedRendererEngine2D::RenderUIPanel( int x, int y, int width, int height, float r, float g, float b, float a )
{
    if ( !square || !uiVertexShader || !plainPixelShader )
        return;

    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pDeviceContext->IASetInputLayout( pInputLayout.Get() );
    SetupVerticesAndShaders( stride, offset, 1, square->renderingData, uiVertexShader.Get(), plainPixelShader.Get() );

    UIPanelCB cb;
    cb.sizeX    = (float)width;
    cb.sizeY    = (float)height;
    cb.posX     = (float)x;
    cb.posY     = (float)y;
    cb.screenW  = (float)screenWidth;
    cb.screenH  = (float)screenHeight;
    cb.color[0] = r;
    cb.color[1] = g;
    cb.color[2] = b;
    cb.color[3] = a;

    pDeviceContext->UpdateSubresource( pConstantBuffer.Get(), 0, nullptr, &cb, 0, 0 );

    {
        ID3D11Buffer* cb = pConstantBuffer.Get();
        pDeviceContext->VSSetConstantBuffers( 0, 1, &cb );
    }

    pDeviceContext->DrawIndexed( square->renderingData->indexCount, 0, 0 );
}

void InstancedRendererEngine2D::ApplyUpgrade( int option )
{
    if ( gameState != GameState::StageClear )
        return;

    switch ( option )
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
        maxAnts = maxAnts + 128;
        break;
    default:
        break;
    }

    AdvanceStage();
}

void InstancedRendererEngine2D::RebuildDepartureStagger()
{
    double sendInterval = max( 0.02, 1.0 / max( 0.01, (double)antsPerSecond ) );
    int queued          = 0;
    for ( int i = 0; i < activeAnts && i < (int)instances.size(); ++i )
    {
        InstanceData& it = instances[i];
        // Only adjust ants that are at the nest (ToNest) to prevent mid-air changes
        if ( it.movementState == 1 )
        {
            it.holdTimer = (float)( sendInterval * queued );
            ++queued;
        }
    }
    if ( pDeviceContext && computeBufferA && computeBufferB )
    {
        pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, nullptr, instances.data(), 0, 0 );
        pDeviceContext->CopyResource( computeBufferB.Get(), computeBufferA.Get() );
    }
}

void InstancedRendererEngine2D::ToggleEndless( bool enabled )
{
    endlessMode = enabled;
}

void InstancedRendererEngine2D::SetFood( int x, int y, float amount )
{
    SpawnFoodAtScreen( x, y, amount );
    // Select the newly spawned node (last)
    SetActiveFoodByIndex( (int)foodNodes.size() - 1 );
}

void InstancedRendererEngine2D::SetNest( int x, int y )
{
    nestPos.x = ( x / (float)screenWidth * 2.0f ) - 1.0f;
    nestPos.y = 1.0f - ( y / (float)screenHeight * 2.0f );
    Vector2D target =
        ( activeFoodIndex >= 0 && activeFoodIndex < (int)foodNodes.size() ) ? foodNodes[activeFoodIndex].pos : nestPos;
    travelTime = ( antSpeed > 0.0f ) ? ( std::sqrt( ( target.x - nestPos.x ) * ( target.x - nestPos.x ) +
                                                    ( target.y - nestPos.y ) * ( target.y - nestPos.y ) ) /
                                         antSpeed )
                                     : 0.0;
}

void InstancedRendererEngine2D::UpdateGame( double dt )
{
    // Update hazard motion
    if ( hazard.active )
    {
        hazard.pos.x += hazard.vel.x * (float)dt;
        hazard.pos.y += hazard.vel.y * (float)dt;
        // Bounce on NDC edges
        if ( hazard.pos.x > 1.0f - hazard.radius )
        {
            hazard.pos.x = 1.0f - hazard.radius;
            hazard.vel.x *= -1.0f;
        }
        if ( hazard.pos.x < -1.0f + hazard.radius )
        {
            hazard.pos.x = -1.0f + hazard.radius;
            hazard.vel.x *= -1.0f;
        }
        if ( hazard.pos.y > 1.0f - hazard.radius )
        {
            hazard.pos.y = 1.0f - hazard.radius;
            hazard.vel.y *= -1.0f;
        }
        if ( hazard.pos.y < -1.0f + hazard.radius )
        {
            hazard.pos.y = -1.0f + hazard.radius;
            hazard.vel.y *= -1.0f;
        }
    }

    // Stage/game timers and random slow event
    if ( gameState == GameState::GameOver )
    {
        return;
    }
    if ( gameState == GameState::StageClear )
    {
        // Idle until player advances
        mode = AntMode::Idle;
        return;
    }
    stageTimeLeft = max( 0.0, stageTimeLeft - dt );
    slowSinceLast += dt;
    if ( !slowActive && slowSinceLast >= slowCooldown )
    {
        // Small chance per second to trigger slow
        if ( RandomGenerator::Generate( 0.0f, 1.0f ) > 0.96f )
        {
            slowActive    = true;
            slowTimeLeft  = 5.0;
            slowSinceLast = 0.0;
        }
    }
    else if ( slowActive )
    {
        slowTimeLeft -= dt;
        if ( slowTimeLeft <= 0.0 )
        {
            slowActive = false;
        }
    }

    // Spawn strictly when food is selected (ToFood state)
    if ( mode == AntMode::ToFood && activeFoodIndex >= 0 )
    {
        int capacity = maxAnts - ( activeAnts + (int)pendingSpawns.size() );
        // Only schedule one pending spawn at a time, using spawnDelaySec from settings
        if ( capacity > 0 && pendingSpawns.empty() )
        {
            double interval = max( 0.01, spawnDelaySec );
            pendingSpawns.push_back( interval );
        }
    }
    // Tick pending spawns and activate
    if ( !pendingSpawns.empty() )
    {
        // Decrease all timers by dt, but only activate one ant per frame
        for ( double& t : pendingSpawns )
            t -= dt;
        if ( pendingSpawns.front() <= 0.0 && activeAnts < maxAnts )
        {
            pendingSpawns.pop_front();
            // Activate one ant at slot = activeAnts
            int slot = activeAnts;
            activeAnts++;
            // Initialize its state near the nest to avoid overlap
            float ang         = RandomGenerator::Generate( 0.0f, 6.28318f );
            float rad         = RandomGenerator::Generate( 0.005f, 0.025f );
            float offX        = ( rad * cosf( ang ) );
            float offY        = ( rad * sinf( ang ) );
            InstanceData init = {};
            init.posX         = nestPos.x + offX;
            init.posY         = nestPos.y + offY;
            init.directionX   = 0.0f;
            init.directionY   = 0.0f;
            init.goalX        = nestPos.x;
            init.goalY        = nestPos.y;
            init.laneOffset   = RandomGenerator::Generate( -0.03f, 0.03f );
            init.speedScale   = RandomGenerator::Generate( 0.85f, 1.15f );
            init.color =
                DirectX::XMFLOAT4( RandomGenerator::Generate( 0.1f, 1.0f ), RandomGenerator::Generate( 0.1f, 1.0f ),
                                   RandomGenerator::Generate( 0.1f, 1.0f ), 1.0f );
            init.movementState = 1; // ToNest (idle at nest)
            init.sourceIndex   = -1;
            init.holdTimer     = (float)max( 0.01, spawnDelaySec );

            // Update compute buffers A and B at the slot
            UINT offsetBytes = slot * sizeof( InstanceData );
            D3D11_BOX box    = {};
            box.left         = offsetBytes;
            box.right        = offsetBytes + sizeof( InstanceData );
            box.top          = 0;
            box.bottom       = 1;
            box.front        = 0;
            box.back         = 1;
            pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, &box, &init, 0, 0 );
            pDeviceContext->UpdateSubresource( computeBufferB.Get(), 0, &box, &init, 0, 0 );
        }
    }

    // Stage transitions should not depend on input/activity
    if ( stageScore >= stageTarget )
    {
        ResetAnts();
        gameState = GameState::StageClear;
    }
    else if ( stageTimeLeft <= 0.0 )
    {
        if ( endlessMode )
        {
            AdvanceStage();
        }
        else
        {
            gameState = GameState::GameOver;
        }
    }

    if ( mode == AntMode::Idle )
        return;

    legElapsed += dt;
    sinceLastDeposit += dt;

    // Scoring handled via nest hit counts in compute shader readback

    if ( antSpeed <= 0.0f )
        return;

    // Remove travelTime-based leg switching; compute shader governs transitions

    // Stage transitions handled above to always trigger, even when idle
}

void InstancedRendererEngine2D::ResetGame()
{
    score            = 0;
    stageScore       = 0;
    stage            = 1;
    gameState        = GameState::Playing;
    combo            = 1;
    sinceLastDeposit = 9999.0;
    slowActive       = false;
    slowSinceLast    = 0.0;
    slowTimeLeft     = 0.0;
    foodNodes.clear();
    activeFoodIndex  = -1;
    activeAnts       = 0;
    spawnAccumulator = 0.0;
}

void InstancedRendererEngine2D::StartStage( int number )
{
    stage         = number;
    stageTarget   = 150 + ( stage - 1 ) * 120;
    stageTimeLeft = 60.0 + ( stage - 1 ) * 10.0;
    antsPerSecond = 20.0f + stage * 10.0f;
    // Do not auto-increase maxAnts across stages; only upgrades change it
    stageScore       = 0;
    combo            = 1;
    sinceLastDeposit = 9999.0;
    slowActive       = false;
    slowSinceLast    = 0.0;
    slowTimeLeft     = 0.0;

    foodNodes.clear();
    int count = min( 3 + stage, 10 );
    SpawnRandomFood( count );
    // Do not auto-select; wait for user selection
    activeFoodIndex = -1;
    mode            = AntMode::Idle;

    // Initialize with configured number of ants (idle at nest)
    activeAnts       = min( maxAnts, initialAnts );
    spawnAccumulator = 0.0;
    pendingSpawns.clear();

    // Reset ants fully
    ResetAnts();
    mode = ( activeFoodIndex >= 0 ) ? AntMode::ToFood : AntMode::Idle;
}

void InstancedRendererEngine2D::AdvanceStage()
{
    gameState = GameState::Playing;
    StartStage( stage + 1 );
}

void InstancedRendererEngine2D::SpawnRandomFood( int count )
{
    for ( int i = 0; i < count; ++i )
    {
        // Try to place a node with non-overlap
        for ( int tries = 0; tries < 100; ++tries )
        {
            float x = RandomGenerator::Generate( -0.9f, 0.9f );
            float y = RandomGenerator::Generate( -0.9f, 0.9f );
            Vector2D p{ x, y };
            bool ok = true;
            for ( const auto& n : foodNodes )
            {
                float dx = n.pos.x - p.x;
                float dy = n.pos.y - p.y;
                if ( ( dx * dx + dy * dy ) < ( minFoodSpacing * minFoodSpacing ) )
                {
                    ok = false;
                    break;
                }
            }
            // Also keep away from nest
            if ( ok )
            {
                float dnx = nestPos.x - p.x;
                float dny = nestPos.y - p.y;
                if ( ( dnx * dnx + dny * dny ) < ( minFoodSpacing * minFoodSpacing ) )
                    ok = false;
            }
            if ( ok )
            {
                // Randomize amount per node by stage
                float minAmt = defaultFoodAmount * ( 0.8f + 0.1f * (float)stage );
                float maxAmt = defaultFoodAmount * ( 1.5f + 0.2f * (float)stage );
                float amt    = RandomGenerator::Generate( minAmt, maxAmt );
                FoodNode n{ p, amt };
                // 30% chance to be triangle
                n.isTriangle = ( RandomGenerator::Generate( 0.0f, 1.0f ) < 0.3f );
                foodNodes.push_back( n );
                break;
            }
        }
    }
    // Do not auto-select here
}

void InstancedRendererEngine2D::SpawnFoodAtScreen( int x, int y, float amount )
{
    Vector2D p;
    p.x = ( x / (float)screenWidth * 2.0f ) - 1.0f;
    p.y = 1.0f - ( y / (float)screenHeight * 2.0f );
    // keep non-overlap
    for ( const auto& n : foodNodes )
    {
        float dx = n.pos.x - p.x;
        float dy = n.pos.y - p.y;
        if ( ( dx * dx + dy * dy ) < ( minFoodSpacing * minFoodSpacing ) )
            return; // too close, skip
    }
    // keep away from nest
    {
        float dnx = nestPos.x - p.x;
        float dny = nestPos.y - p.y;
        if ( ( dnx * dnx + dny * dny ) < ( minFoodSpacing * minFoodSpacing ) )
            return;
    }
    foodNodes.push_back( FoodNode{ p, amount } );
}

int InstancedRendererEngine2D::FindNearestFoodScreen( int x, int y, float maxPixelRadius ) const
{
    if ( foodNodes.empty() )
        return -1;
    // Pixel-space hit test: axis-aligned rectangle matching rendered marker (top-left anchored)
    int cx       = x;
    int cy       = y;
    float base   = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
    int best     = -1;
    float bestD2 = 1e12f;
    for ( size_t i = 0; i < foodNodes.size(); ++i )
    {
        const auto& node = foodNodes[i];
        float sx         = ( node.pos.x + 1.0f ) * 0.5f * (float)screenWidth;
        float sy         = ( 1.0f - node.pos.y ) * 0.5f * (float)screenHeight;
        float ratio      = node.amount / base;
        ratio            = max( 0.3f, min( ratio, 2.5f ) );
        // width/height in pixels match shader math: base quad 0.05, size=2*ratio, aspect divides X
        float widthPx  = ( 0.05f * 2.0f * ratio / aspectRatioX ) * ( (float)screenWidth * 0.5f );
        float heightPx = ( 0.05f * 2.0f * ratio ) * ( (float)screenHeight * 0.5f );
        if ( cx >= sx && cx <= sx + widthPx && cy >= sy && cy <= sy + heightPx )
        {
            float dx = ( sx + widthPx * 0.5f ) - cx;
            float dy = ( sy + heightPx * 0.5f ) - cy;
            float d2 = dx * dx + dy * dy;
            if ( d2 < bestD2 )
            {
                bestD2 = d2;
                best   = (int)i;
            }
        }
    }
    return best;
}

void InstancedRendererEngine2D::SetActiveFoodByIndex( int index )
{
    if ( index < 0 )
    {
        // Deselect: keep current journeys; ants at nest will stay put
        activeFoodIndex = -1;
        return;
    }
    if ( index >= (int)foodNodes.size() )
        return;
    activeFoodIndex = index;
    mode            = AntMode::ToFood;
    legElapsed      = 0.0;
    Vector2D target = foodNodes[activeFoodIndex].pos;
    travelTime      = ( antSpeed > 0.0f ) ? ( std::sqrt( ( target.x - nestPos.x ) * ( target.x - nestPos.x ) +
                                                         ( target.y - nestPos.y ) * ( target.y - nestPos.y ) ) /
                                         antSpeed )
                                          : 0.0;
    int fx          = (int)( ( target.x + 1.0f ) * 0.5f * screenWidth );
    int fy          = (int)( ( 1.0f - target.y ) * 0.5f * screenHeight );
    SetFlockTarget( fx, fy );
}
void InstancedRendererEngine2D::RenderFoodMarkers()
{
    float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
    for ( size_t i = 0; i < foodNodes.size(); ++i )
    {
        const auto& node = foodNodes[i];
        float ratio      = node.amount / base;
        ratio            = max( 0.3f, min( ratio, 2.5f ) );
        int color        = ( static_cast<int>( i ) == activeFoodIndex ) ? 3 : 1;
        if ( node.isTriangle && triangle && triangle->renderingData )
        {
            // TriangleMesh is centered horizontally; adjust if needed by using TL anchor directly
            RenderMarkerWithMesh( triangle->renderingData, node.pos, 2.0f * ratio, 2.0f * ratio, color );
        }
        else
        {
            RenderMarker( node.pos, 2.0f * ratio, color );
        }
    }
}

void InstancedRendererEngine2D::RenderFoodLabels()
{
    float base = defaultFoodAmount > 1e-5f ? defaultFoodAmount : 100.0f;
    if ( !ImGui::GetCurrentContext() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font   = ImGui::GetFont();
    float size     = ImGui::GetFontSize() * 1.2f;
    for ( const auto& node : foodNodes )
    {
        int sx = (int)( ( node.pos.x + 1.0f ) * 0.5f * (float)screenWidth );
        int sy = (int)( ( 1.0f - node.pos.y ) * 0.5f * (float)screenHeight );
        wchar_t wbuf[64];
        swprintf_s( wbuf, L"%.0f", max( 0.0f, node.amount ) );
        char buf[64];
        wcstombs( buf, wbuf, sizeof( buf ) );
        ImVec2 ts = ImGui::CalcTextSize( buf );
        ImVec2 pos( (float)sx - ts.x * 0.5f, (float)sy - ts.y * 0.5f );
        dl->AddText( font, size, ImVec2( pos.x + 1, pos.y + 1 ), IM_COL32( 0, 0, 0, 180 ), buf );
        dl->AddText( font, size, pos, IM_COL32( 255, 255, 255, 230 ), buf );
    }
}

void InstancedRendererEngine2D::RenderProminentStatus()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    int totalSec = (int)std::ceil( max( 0.0, stageTimeLeft ) );
    int mm       = totalSec / 60;
    int ss       = totalSec % 60;

    wchar_t w1[64];
    wchar_t w2[64];
    wchar_t w3[64];
    swprintf_s( w1, L"SCORE: %d", score );
    swprintf_s( w2, L"TIME LEFT: %02d:%02d", mm, ss );
    swprintf_s( w3, L"TARGET: %d / %d", stageScore, stageTarget );
    char l1[96];
    char l2[96];
    char l3[96];
    wcstombs( l1, w1, sizeof( l1 ) );
    wcstombs( l2, w2, sizeof( l2 ) );
    wcstombs( l3, w3, sizeof( l3 ) );

    // Measure and build a centered banner-like panel (distinct style from top-right)
    float titleSize = 32.0f; // slightly smaller than before to fit a panel
    float lineSize  = 22.0f;
    float scaleT    = titleSize / ImGui::GetFontSize();
    float scaleL    = lineSize / ImGui::GetFontSize();
    ImVec2 s1       = ImGui::CalcTextSize( l1 );
    s1.x *= scaleT;
    s1.y *= scaleT;
    ImVec2 s2 = ImGui::CalcTextSize( l2 );
    s2.x *= scaleL;
    s2.y *= scaleL;
    ImVec2 s3 = ImGui::CalcTextSize( l3 );
    s3.x *= scaleL;
    s3.y *= scaleL;

    float padX   = 18.0f;
    float padY   = 10.0f;
    float gap    = 6.0f;
    float panelW = max( s1.x, max( s2.x, s3.x ) ) + padX * 2.0f;
    float panelH = ( s1.y + s2.y + s3.y ) + padY * 2.0f + gap * 2.0f;

    ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImVec2 p1( disp.x * 0.5f - panelW * 0.5f, 10.0f );
    ImVec2 p2( p1.x + panelW, p1.y + panelH );

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();
    // Distinct style: dark slate with subtle border
    DrawRoundedBackground( dl, p1, p2, IM_COL32( 28, 30, 38, 215 ), IM_COL32( 255, 255, 255, 30 ), 10.0f );

    float x = p1.x + padX;
    float y = p1.y + padY;
    dl->AddText( f, titleSize, ImVec2( x, y ), IM_COL32( 255, 255, 255, 245 ), l1 );
    y += s1.y + gap;
    dl->AddText( f, lineSize, ImVec2( x, y ), IM_COL32( 230, 230, 230, 235 ), l2 );
    y += s2.y + gap;
    dl->AddText( f, lineSize, ImVec2( x, y ), IM_COL32( 230, 230, 230, 235 ), l3 );
}

void InstancedRendererEngine2D::RenderEventBanner()
{
    if ( !ImGui::GetCurrentContext() )
        return;

    // Combine active events into a single banner
    const char* parts[3];
    int n = 0;
    if ( slowActive )
        parts[n++] = "Rain - ants slowed";
    if ( frenzyActive )
        parts[n++] = "Frenzy!";
    if ( hazard.active )
        parts[n++] = "Hazard active";
    if ( n == 0 )
        return;

    std::string label;
    for ( int i = 0; i < n; ++i )
    {
        if ( i )
            label += "  |  ";
        label += parts[i];
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();
    ImVec2 disp    = ImGui::GetIO().DisplaySize;

    float fontSize = 18.0f;
    float scale    = fontSize / ImGui::GetFontSize();
    ImVec2 ts      = ImGui::CalcTextSize( label.c_str() );
    ImVec2 tss     = ImVec2( ts.x * scale, ts.y * scale );

    float padX = 12.0f;
    float padY = 6.0f;
    ImVec2 sz  = ImVec2( tss.x + padX * 2.0f, tss.y + padY * 2.0f );

    // Right-align the banner: x = width - bannerWidth - margin
    float margin = 16.0f;
    ImVec2 p1    = ImVec2( disp.x - margin - sz.x, 16.0f );
    ImVec2 p2    = ImVec2( p1.x + sz.x, p1.y + sz.y );

    // Choose a background color: priority Frenzy > Hazard > Rain, else neutral
    ImU32 bg = IM_COL32( 32, 32, 42, 210 );
    if ( frenzyActive )
        bg = IM_COL32( 240, 80, 40, 210 );
    else if ( hazard.active )
        bg = IM_COL32( 240, 190, 40, 210 );
    else if ( slowActive )
        bg = IM_COL32( 40, 120, 220, 210 );
    ImU32 fg = ( hazard.active && !frenzyActive ) ? IM_COL32( 20, 20, 20, 255 ) : IM_COL32( 255, 255, 255, 240 );

    float r = 8.0f;
    // Shadow
    dl->AddRectFilled( ImVec2( p1.x + 2, p1.y + 2 ), ImVec2( p2.x + 2, p2.y + 2 ), IM_COL32( 0, 0, 0, 120 ), r );
    // Banner
    dl->AddRectFilled( p1, p2, bg, r );
    dl->AddRect( p1, p2, IM_COL32( 0, 0, 0, 100 ), r );
    // Text
    ImVec2 tp = ImVec2( p1.x + padX, p1.y + padY );
    dl->AddText( f, fontSize, tp, fg, label.c_str() );
}

void InstancedRendererEngine2D::RenderTopLeftStats()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();

    // Build content
    char buf1[64];
    char buf2[64];
    snprintf( buf1, sizeof( buf1 ), "Stage: %d", stage );
    snprintf( buf2, sizeof( buf2 ), "Ants: %d / %d", activeAnts, maxAnts );

    float lineSize = 18.0f;
    float scale    = lineSize / ImGui::GetFontSize();
    ImVec2 s1      = ImGui::CalcTextSize( buf1 );
    s1.x *= scale;
    s1.y *= scale;
    ImVec2 s2 = ImGui::CalcTextSize( buf2 );
    s2.x *= scale;
    s2.y *= scale;

    float padX = 12.0f;
    float padY = 8.0f;
    float gap  = 4.0f;
    float w    = max( s1.x, s2.x ) + padX * 2.0f;
    float h    = ( s1.y + s2.y ) + padY * 2.0f + gap;

    ImVec2 p1( 16.0f, 16.0f );
    ImVec2 p2( p1.x + w, p1.y + h );
    DrawRoundedBackground( dl, p1, p2, IM_COL32( 28, 30, 38, 200 ), IM_COL32( 255, 255, 255, 28 ), 8.0f );

    float x = p1.x + padX;
    float y = p1.y + padY;
    dl->AddText( f, lineSize, ImVec2( x, y ), IM_COL32( 235, 235, 235, 240 ), buf1 );
    y += s1.y + gap;
    dl->AddText( f, lineSize, ImVec2( x, y ), IM_COL32( 235, 235, 235, 240 ), buf2 );
}

void InstancedRendererEngine2D::RenderStageClearOverlay()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();
    ImVec2 disp    = ImGui::GetIO().DisplaySize;

    const char* title    = "Stage Clear!";
    const char* subtitle = "Choose Upgrade";
    float titleSize      = 28.0f;
    float subtitleSize   = 20.0f;
    float headerSize     = 18.0f; // header above each button
    float btnW           = disp.x / 6.0f;
    float btnH           = 44.0f;
    float gap            = 10.0f;
    float padY           = 16.0f;
    float panelPadding   = 32.0f;

    // Measure title and subtitle
    float scaleT = titleSize / ImGui::GetFontSize();
    ImVec2 ts    = ImGui::CalcTextSize( title );
    ts.x *= scaleT;
    ts.y *= scaleT;

    float scaleS = subtitleSize / ImGui::GetFontSize();
    ImVec2 ssb   = ImGui::CalcTextSize( subtitle );
    ssb.x *= scaleS;
    ssb.y *= scaleS;

    float rowW     = btnW * 3.0f + gap * 2.0f;
    float contentW = max( max( ts.x, ssb.x ), rowW );
    float contentH = ts.y + 6.0f + ssb.y + 12.0f + headerSize + 6.0f + btnH; // title + sub + headers + buttons

    // Backdrop
    dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( disp.x, disp.y ), IM_COL32( 0, 0, 0, 120 ) );

    // Panel
    ImVec2 pos( disp.x / 2.0f - ( contentW / 2 ), disp.y / 3.0f );
    ImVec2 size( pos.x + contentW, pos.y + contentH + padY * 3.0f );

    ImVec2 panelPos( pos.x - panelPadding, pos.y );
    ImVec2 panelSize( size.x + panelPadding, size.y );
    DrawRoundedBackground( dl, panelPos, panelSize, IM_COL32( 28, 30, 38, 225 ), IM_COL32( 255, 255, 255, 30 ), 10.0f );

    float textStartY = pos.y + padY;

    // Center title and subtitle horizontally within panel
    float titleXPos = pos.x + ( contentW - ts.x ) * 0.5f;
    dl->AddText( f, titleSize, ImVec2( titleXPos, textStartY ), IM_COL32( 255, 255, 255, 245 ), title );
    textStartY += ts.y + 6.0f;

    float subTitleXPos = pos.x + ( (contentW)-ssb.x ) * 0.5f;
    dl->AddText( f, subtitleSize, ImVec2( subTitleXPos, textStartY ), IM_COL32( 230, 230, 230, 240 ), subtitle );
    textStartY += ssb.y + 12.0f;

    // Buttons row (3) with headers
    Button buttons[]     = { { "Speed", "+15%", 1 }, { "Spawn Rate", "+25%", 2 }, { "Max Ants", "+128", 3 } };
    float buttonStartX   = pos.x + ( (contentW)-rowW ) * 0.5f;
    bool clicked         = ImGui::IsMouseClicked( ImGuiMouseButton_Left );
    ImVec2 mousePosition = ImGui::GetIO().MousePos;

    float scaleH = headerSize / ImGui::GetFontSize();
    for ( int i = 0; i < 3; ++i )
    {
        const auto& b = buttons[i];

        // Header centered above each button
        ImVec2 hsz = ImGui::CalcTextSize( b.header );
        hsz.x *= scaleH;
        hsz.y *= scaleH;
        float hx = buttonStartX + ( btnW - hsz.x ) * 0.5f;
        dl->AddText( f, headerSize, ImVec2( hx, textStartY ), IM_COL32( 200, 220, 255, 240 ), b.header );

        // Button
        ImVec2 b1( buttonStartX, textStartY + headerSize + 6.0f );
        ImVec2 b2( buttonStartX + btnW, b1.y + btnH );

        bool hover = ( mousePosition.x >= b1.x && mousePosition.x <= b2.x && mousePosition.y >= b1.y &&
                       mousePosition.y <= b2.y );
        ImU32 bg   = hover ? IM_COL32( 70, 90, 130, 255 ) : IM_COL32( 60, 78, 110, 255 );
        DrawRoundedBackground( dl, b1, b2, bg, IM_COL32( 255, 255, 255, 40 ), 6.0f );

        // Label centered with scaling
        float buttonFontSize = 22.0f;
        float scale          = buttonFontSize / ImGui::GetFontSize();
        ImVec2 t             = ImGui::CalcTextSize( b.label );
        t.x *= scale;
        t.y *= scale;
        float lx = b1.x + ( btnW - t.x ) * 0.5f;
        float ly = b1.y + ( btnH - t.y ) * 0.5f;

        dl->AddText( f, buttonFontSize, ImVec2( lx, ly ), IM_COL32( 255, 255, 255, 240 ), b.label );

        if ( hover && clicked )
        {
            ApplyUpgrade( b.id );
            return;
        }

        buttonStartX += btnW + gap;
    }
}

void InstancedRendererEngine2D::RenderGameOverOverlay()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();
    ImVec2 disp    = ImGui::GetIO().DisplaySize;

    const char* title = "Game Over!";
    float titleSize   = 28.0f;
    float btnW        = 180.0f;
    float btnH        = 42.0f;
    float gap         = 12.0f;
    float padX        = 18.0f;
    float padY        = 14.0f;

    float scaleT = titleSize / ImGui::GetFontSize();
    ImVec2 ts    = ImGui::CalcTextSize( title );
    ts.x *= scaleT;
    ts.y *= scaleT;

    float contentW = max( ts.x, btnW * 2.0f + gap );
    float contentH = ts.y + gap + btnH + padY * 2.0f;

    // Backdrop
    dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( disp.x, disp.y ), IM_COL32( 0, 0, 0, 120 ) );

    ImVec2 p1( disp.x * 0.5f - ( contentW + padX * 2.0f ) * 0.5f, disp.y * 0.5f - ( contentH + padY * 2.0f ) * 0.5f );
    ImVec2 p2( p1.x + contentW + padX * 2.0f, p1.y + contentH + padY * 2.0f );
    DrawRoundedBackground( dl, p1, p2, IM_COL32( 28, 30, 38, 225 ), IM_COL32( 255, 255, 255, 30 ), 10.0f );

    float x = p1.x + padX;
    float y = p1.y + padY;
    dl->AddText( f, titleSize, ImVec2( x, y ), IM_COL32( 255, 255, 255, 245 ), title );
    y += ts.y + gap;

    struct Button
    {
        const char* label;
        int action;
    };
    enum
    {
        Restart = 1,
        Endless = 2
    };
    Button buttons[] = { { "Restart", Restart }, { "Endless Mode", Endless } };

    bool clicked = ImGui::IsMouseClicked( ImGuiMouseButton_Left );
    ImVec2 mp    = ImGui::GetIO().MousePos;
    float rowW   = btnW * 2.0f + gap;
    float bx     = p1.x + ( (contentW)-rowW ) * 0.5f;
    for ( const auto& b : buttons )
    {
        ImVec2 b1( bx, y );
        ImVec2 b2( bx + btnW, y + btnH );
        bool hover = ( mp.x >= b1.x && mp.x <= b2.x && mp.y >= b1.y && mp.y <= b2.y );
        ImU32 bg   = hover ? IM_COL32( 90, 70, 70, 255 ) : IM_COL32( 80, 60, 60, 255 );
        DrawRoundedBackground( dl, b1, b2, bg, IM_COL32( 255, 255, 255, 40 ), 6.0f );
        float lsz   = 20.0f;
        float scale = lsz / ImGui::GetFontSize();
        ImVec2 ls   = ImGui::CalcTextSize( b.label );
        ls.x *= scale;
        ls.y *= scale;
        float lx = b1.x + ( btnW - ls.x ) * 0.5f;
        float ly = b1.y + ( btnH - ls.y ) * 0.5f;
        dl->AddText( f, lsz, ImVec2( lx, ly ), IM_COL32( 255, 255, 255, 240 ), b.label );
        if ( hover && clicked )
        {
            if ( b.action == Restart )
            {
                ResetGame();
                StartStage( 1 );
            }
            else if ( b.action == Endless )
            {
                ToggleEndless( true );
                AdvanceStage();
            }
            return;
        }
        bx += btnW + gap;
    }
}

void InstancedRendererEngine2D::RenderPheromonePath()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    if ( activeFoodIndex < 0 || activeFoodIndex >= (int)foodNodes.size() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 disp    = ImGui::GetIO().DisplaySize;

    auto toScreen = [&]( const Vector2D& p )
    {
        float x = ( p.x + 1.0f ) * 0.5f * disp.x;
        float y = ( 1.0f - p.y ) * 0.5f * disp.y;
        return ImVec2( x, y );
    };

    ImVec2 a  = toScreen( nestPos );
    ImVec2 b  = toScreen( foodNodes[activeFoodIndex].pos );
    ImU32 col = IM_COL32( 60, 220, 180, 120 );
    if ( partyMode )
        col = IM_COL32( 255, 180, 80, 140 );
    dl->AddLine( a, b, col, 3.0f );
}

void InstancedRendererEngine2D::SpawnBonusSugarAtScreen( int x, int y, float amount, float mult, float decay )
{
    Vector2D p;
    p.x = ( x / (float)screenWidth * 2.0f ) - 1.0f;
    p.y = 1.0f - ( y / (float)screenHeight * 2.0f );
    // non-overlap and away from nest
    for ( const auto& n : foodNodes )
    {
        float dx = n.pos.x - p.x;
        float dy = n.pos.y - p.y;
        if ( ( dx * dx + dy * dy ) < ( minFoodSpacing * minFoodSpacing ) )
            return;
    }
    float dnx = nestPos.x - p.x;
    float dny = nestPos.y - p.y;
    if ( ( dnx * dnx + dny * dny ) < ( minFoodSpacing * minFoodSpacing ) )
        return;
    FoodNode n{ p, amount };
    n.isTriangle      = true;
    n.isBonus         = true;
    n.scoreMultiplier = mult;
    n.decayPerSecond  = decay;
    foodNodes.push_back( n );
}

void InstancedRendererEngine2D::TriggerConfettiBurst( int x, int y, int count )
{
    for ( int i = 0; i < count; ++i )
    {
        PartyParticle p{};
        p.x       = static_cast<float>( x );
        p.y       = static_cast<float>( y );
        float ang = RandomGenerator::Generate( 0.0f, 6.2831853f );
        float spd = RandomGenerator::Generate( 120.0f, 420.0f );
        p.vx      = std::cos( ang ) * spd;
        p.vy      = std::sin( ang ) * spd - RandomGenerator::Generate( 80.0f, 160.0f );
        p.ttl     = RandomGenerator::Generate( 1.0f, 2.0f );
        p.life    = p.ttl;
        int r     = static_cast<int>( RandomGenerator::Generate( 80.0f, 255.0f ) );
        int g     = static_cast<int>( RandomGenerator::Generate( 80.0f, 255.0f ) );
        int b     = static_cast<int>( RandomGenerator::Generate( 80.0f, 255.0f ) );
        int a     = 230;
        p.color   = IM_COL32( r, g, b, a );
        p.shape   = static_cast<int>( RandomGenerator::Generate( 0.0f, 2.99f ) );
        p.size    = RandomGenerator::Generate( 4.0f, 10.0f );
        p.rot     = RandomGenerator::Generate( 0.0f, 6.2831853f );
        p.rotVel  = RandomGenerator::Generate( -6.0f, 6.0f );
        partyParticles.emplace_back( p );
    }
}

int InstancedRendererEngine2D::GetActiveFoodIndex() const
{
    return activeFoodIndex;
}
