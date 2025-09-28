#include "InstancedRendererEngine2D.h"

#include "Game/AntGame.h"

#include <stdexcept>

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

void InstancedRendererEngine2D::SetGame( Game::AntGame* game )
{
    game_ = game;
}

void InstancedRendererEngine2D::Init( HWND windowHandle, int blockWidth, int blockHeight )
{
    if ( !game_ )
    {
        throw std::runtime_error( "InstancedRendererEngine2D::Init called before game was bound" );
    }

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
    // per file for compiling model etc...
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

    // Create instance buffer containing per-ant state
    UINT instanceCount = 1000;
    game_->state().instances.reserve( instanceCount );

    for ( UINT i = 0; i < instanceCount; ++i )
    {
        InstanceData instance{};

        // Start at nest with a tiny jitter to reduce initial overlap
        instance.posX       = game_->state().nestPos.x;
        instance.posY       = game_->state().nestPos.y;
        instance.directionX = 0;
        instance.directionY = 0;
        instance.goalX      = game_->state().nestPos.x;
        instance.goalY      = game_->state().nestPos.y;
        instance.laneOffset = 0.03f;
        instance.speedScale = 1.15f;
        instance.holdTimer  = 0.0f;
        instance.color = DirectX::XMFLOAT4( 1.0f, 1.0f, 1.0f,1.0f );
        instance.movementState = 0; // start heading to food
        instance.sourceIndex   = -1;

        game_->state().instances.emplace( game_->state().instances.begin() + i, instance );
    }

    // ImGui wrapper
    imgui = std::make_unique<ImGuiRenderer>();
    imgui->Init( windowHandle, pDevice.Get(), pDeviceContext.Get() );

    // Load settings (optional file)
    CreateMeshes();
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

    if ( !game_ )
    {
        return;
    }

    // Define a darker sand-like background to reduce brightness.
    constexpr float clearColor[4] = { 0.55f, 0.50f, 0.40f, 1.0f };

    pDeviceContext->ClearRenderTargetView( renderTargetView.Get(), clearColor ); // Clear the back buffer.

    game_->update( deltaTime );

    // Hover outline overlay on top of fills
    POINT mp;
    GetCursorPos( &mp );
    ScreenToClient( windowHandle, &mp );

    // ImGui frame & UI
    if ( imgui )
    {
        imgui->NewFrame( static_cast<float>(screenWidth), static_cast<float>(screenHeight), deltaTime );
        // Prominent status text (game_->state().score + time) centered at the top
        RenderProminentStatus();
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
    pDeviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
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

    vp.Width    = static_cast<FLOAT>(newWidth);
    vp.Height   = static_cast<FLOAT>(newHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    pDeviceContext->RSSetViewports( 1, &vp );

    // Used for correcting triangles to their original shape and ignoring the aspect ratio of the viewport
    aspectRatioX = ( newHeight > 0 ) ? static_cast<float>(newWidth) / static_cast<float>(newHeight) : 1.0f;
}

Vector2D InstancedRendererEngine2D::ScreenToWorld( int x, int y ) const
{
    if ( screenWidth == 0 || screenHeight == 0 )
        return { cameraPosition.x, cameraPosition.y };

    float fx = static_cast<float>( x ) / static_cast<float>( screenWidth );
    float fy = static_cast<float>( y ) / static_cast<float>( screenHeight );

    float vx = fx * 2.0f - 1.0f;
    float vy = 1.0f - fy * 2.0f;

    return { vx + cameraPosition.x, vy + cameraPosition.y };
}

Vector2D InstancedRendererEngine2D::WorldToView( const Vector2D& world ) const
{
    return { world.x - cameraPosition.x, world.y - cameraPosition.y };
}

Vector2D InstancedRendererEngine2D::WorldToScreen( const Vector2D& world ) const
{
    if ( screenWidth == 0 || screenHeight == 0 ) return { 0.0f, 0.0f };

    Vector2D view = WorldToView( world );
    float px      = ( view.x + 1.0f ) * 0.5f * static_cast<float>( screenWidth );
    float py      = ( 1.0f - view.y ) * 0.5f * static_cast<float>( screenHeight );

    return { px, py };
}

void InstancedRendererEngine2D::InitRenderBufferAndTargetView( HRESULT& hr )
{
    hr = pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( backBuffer.ReleaseAndGetAddressOf() ) );
    if ( FAILED( hr ) )
    {
        throw std::runtime_error( "Failed to create backBuffer" );
    }

    hr = pDevice->CreateRenderTargetView( backBuffer.Get(), nullptr, renderTargetView.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
    {
        throw std::runtime_error( "Failed to create renderTargetView" );
    }

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
}

void InstancedRendererEngine2D::CountFps()
{
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

void InstancedRendererEngine2D::CreateMeshes()
{
    square   = std::make_unique<SquareMesh>( *pDevice.Get() );
    triangle = std::make_unique<TriangleMesh>( *pDevice.Get() );
}

void InstancedRendererEngine2D::CreateBuffers( const std::vector<InstanceData>& instances )
{
    // Create a buffer description for vertices
    D3D11_BUFFER_DESC bd = {};

    // Re-use a buffer description for the input layout like position.
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( VertexInputData );
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT hr = pDevice->CreateBuffer( &bd, nullptr, pConstantBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create constant buffer" );

    hr         = pDevice->CreateBuffer( &bd, nullptr, flockConstantBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create constant buffer" );


    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // Use linear filtering for smooth scaling
    samplerDesc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD             = 0;
    samplerDesc.MaxLOD             = D3D11_FLOAT32_MAX;

    UINT instanceCount = static_cast<UINT>( instances.size() );

    // Create an instance buffer based on the instance vector
    D3D11_BUFFER_DESC instanceBufferDesc   = {};
    instanceBufferDesc.Usage               = D3D11_USAGE_DEFAULT;
    instanceBufferDesc.ByteWidth           = sizeof( InstanceData ) * instanceCount;
    instanceBufferDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    instanceBufferDesc.CPUAccessFlags      = 0; // No flags set
    instanceBufferDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    instanceBufferDesc.StructureByteStride = sizeof( InstanceData );

    hr = pDevice->CreateBuffer( &instanceBufferDesc, nullptr, computeBufferA.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create computerBuffer A" );

    hr = pDevice->CreateBuffer( &instanceBufferDesc, nullptr, computeBufferB.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create computerBuffer B" );

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription = {};
    shaderResourceViewDescription.Format                          = DXGI_FORMAT_UNKNOWN;
    shaderResourceViewDescription.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFER;
    shaderResourceViewDescription.Buffer.ElementOffset            = 0;
    shaderResourceViewDescription.Buffer.NumElements              = instanceCount;

    if ( computeBufferA == nullptr )
        throw std::runtime_error( "computerBufferA == null" );
    if ( computeBufferB == nullptr )
        throw std::runtime_error( "computerBufferB == null" );

    hr = pDevice->CreateShaderResourceView( computeBufferA.Get(), &shaderResourceViewDescription, shaderResourceViewA.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create shaderResourceView A" );

    hr = pDevice->CreateShaderResourceView( computeBufferB.Get(), &shaderResourceViewDescription, shaderResourceViewB.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create shaderResourceView B" );

    D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDescription = {};
    unorderedAccessViewDescription.Format                           = DXGI_FORMAT_UNKNOWN;
    unorderedAccessViewDescription.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
    unorderedAccessViewDescription.Buffer.FirstElement              = 0;
    unorderedAccessViewDescription.Buffer.NumElements               = instanceCount;

    hr = pDevice->CreateUnorderedAccessView( computeBufferA.Get(), &unorderedAccessViewDescription,
                                             unorderedAccessViewA.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create unorderedAccessView A" );

    hr = pDevice->CreateUnorderedAccessView( computeBufferB.Get(), &unorderedAccessViewDescription,
                                             unorderedAccessViewB.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create unorderedAccessView B" );

    // Actually send the initial random positions to the compute shader buffers
    if ( !instances.empty() )
    {
        pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, nullptr, instances.data(), 0, 0 );
        pDeviceContext->CopyResource( computeBufferB.Get(), computeBufferA.Get() );
    }

    // Create a simple instance buffer to satisfy any copy usage
    D3D11_BUFFER_DESC instanceVBDesc = {};
    instanceVBDesc.Usage             = D3D11_USAGE_DEFAULT;
    instanceVBDesc.ByteWidth         = sizeof( InstanceData ) * instanceCount;
    instanceVBDesc.BindFlags         = D3D11_BIND_VERTEX_BUFFER;
    instanceVBDesc.CPUAccessFlags    = 0;
    instanceVBDesc.MiscFlags         = 0;

    hr = pDevice->CreateBuffer( &instanceVBDesc, nullptr, instanceBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create instanceBuffer" );

    // Food hit counts buffer (uint per node)
    D3D11_BUFFER_DESC countDesc   = {};
    countDesc.Usage               = D3D11_USAGE_DEFAULT;
    countDesc.ByteWidth           = sizeof( UINT ) * MaxFoodNodes * HitBins;
    countDesc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    countDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    countDesc.StructureByteStride = sizeof( UINT );
    hr = pDevice->CreateBuffer( &countDesc, nullptr, foodCountBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create foodCountBuffer" );

    unorderedAccessViewDescription.Format                           = DXGI_FORMAT_UNKNOWN;
    unorderedAccessViewDescription.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
    unorderedAccessViewDescription.Buffer.FirstElement              = 0;
    unorderedAccessViewDescription.Buffer.NumElements               = MaxFoodNodes;
    hr = pDevice->CreateUnorderedAccessView( foodCountBuffer.Get(), &unorderedAccessViewDescription, foodCountUAV.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create foodCountUAV" );

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
    D3D11_QUERY_DESC queryDescription = {};
    queryDescription.Query            = D3D11_QUERY_EVENT;
    queryDescription.MiscFlags        = 0;
    for ( int i = 0; i < 3; i++ )
    {
        hr = pDevice->CreateQuery( &queryDescription, &foodQuery[i] );
        if ( FAILED( hr ) )
            throw std::runtime_error( "Failed to create foodQuery" );
    }

    // Nest hit counts buffer
    hr = pDevice->CreateBuffer( &countDesc, nullptr, nestCountBuffer.ReleaseAndGetAddressOf() );
    if ( FAILED( hr ) )
        throw std::runtime_error( "Failed to create nestCountBuffer" );
    hr = pDevice->CreateUnorderedAccessView( nestCountBuffer.Get(), &unorderedAccessViewDescription, nestCountUAV.ReleaseAndGetAddressOf() );
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
        hr = pDevice->CreateQuery( &queryDescription, &nestQuery[i] );
        if ( FAILED( hr ) )
            throw std::runtime_error( "Failed to create nestQuery" );
    }
}

void InstancedRendererEngine2D::InitializeSimulationBuffers( const std::vector<InstanceData>& instances )
{
    CreateBuffers( instances );
}

void InstancedRendererEngine2D::UploadInstanceBuffer( const std::vector<InstanceData>& instances )
{
    if ( !pDeviceContext || !computeBufferA || !computeBufferB || !instanceBuffer )
        return;

    if ( instances.empty() )
        return;

    const UINT requiredBytes = static_cast<UINT>( instances.size() * sizeof( InstanceData ) );

    D3D11_BUFFER_DESC desc{};
    computeBufferA->GetDesc( &desc );
    if ( desc.ByteWidth < requiredBytes )
    {
        ResizeInstanceStorage( instances );
    }

    pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, nullptr, instances.data(), 0, 0 );
    pDeviceContext->CopyResource( computeBufferB.Get(), computeBufferA.Get() );
    pDeviceContext->CopyResource( instanceBuffer.Get(), computeBufferB.Get() );
}

void InstancedRendererEngine2D::UploadInstanceSlot( int slot, const InstanceData& data ) const
{
    if ( !pDeviceContext || !computeBufferA || !computeBufferB || !instanceBuffer )
        return;

    if ( slot < 0 )
        return;

    UINT offsetBytes = static_cast<UINT>( slot ) * sizeof( InstanceData );
    D3D11_BUFFER_DESC desc{};
    computeBufferA->GetDesc( &desc );
    if ( offsetBytes + sizeof( InstanceData ) > desc.ByteWidth )
    {
        // Fall back to full upload when a slot is out of range.
        return;
    }

    D3D11_BOX box{};
    box.left   = offsetBytes;
    box.right  = offsetBytes + sizeof( InstanceData );
    box.top    = 0;
    box.bottom = 1;
    box.front  = 0;
    box.back   = 1;

    pDeviceContext->UpdateSubresource( computeBufferA.Get(), 0, &box, &data, 0, 0 );
    pDeviceContext->UpdateSubresource( computeBufferB.Get(), 0, &box, &data, 0, 0 );
}

void InstancedRendererEngine2D::ResizeInstanceStorage( const std::vector<InstanceData>& instances )
{
    CreateBuffers( instances );
}

void InstancedRendererEngine2D::LoadShaders()
{
    HRESULT hr            = S_FALSE;
    ID3DBlob* flockVsBlob = nullptr;
    ID3DBlob* colorVsBlob = nullptr;
    ID3DBlob* errorBlob   = nullptr;

    const wchar_t* shaderFilePath = L"SquareWaveVertexShader.cso"; // Create a wave vertex shader
    if ( !Utilities::CreateVertexShader( pDevice.Get(), hr, shaderFilePath, &waveVertexShader, nullptr ) )
    {
        return;
    };

    shaderFilePath = L"UIPanelVertexShader.cso"; // Create a UI panel vertex shader
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
        D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
                                  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        // No per-instance data for color VS, but keep a slot reserved if needed later
    };

    // Create the color input layout using the Color VS signature
    hr = pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), colorVsBlob->GetBufferPointer(),
                                     colorVsBlob->GetBufferSize(), &pInputLayout );
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
        { "INSTANCEPOS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 } };

    hr = pDevice->CreateInputLayout( flockLayout, ARRAYSIZE( flockLayout ), flockVsBlob->GetBufferPointer(),
                                     flockVsBlob->GetBufferSize(),
                                     &flockInputLayout // Create the dedicated layout
    );

    if ( FAILED( hr ) ) throw std::runtime_error( "Failed to create input layout for flock" );


    SafeRelease( &flockVsBlob );
    SafeRelease( &colorVsBlob );
    SafeRelease( &errorBlob );
}

void InstancedRendererEngine2D::SetupVerticesAndShaders( const UINT& stride, const UINT& offset, const Mesh* mesh, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader ) const
{
    pDeviceContext->IASetVertexBuffers( 0, 1, &mesh->vertexBuffer, &stride, &offset );
    pDeviceContext->IASetIndexBuffer( mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0 );

    pDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST ); // Define what primitive we should draw with the vertex and indices

    pDeviceContext->VSSetShader( vertexShader, nullptr, 0 );
    pDeviceContext->PSSetShader( pixelShader, nullptr, 0 );
}

void InstancedRendererEngine2D::PassInputDataAndRunInstanced( ID3D11Buffer* buffer, VertexInputData& cbData, Mesh& mesh, int instanceCount ) const
{
    pDeviceContext->UpdateSubresource( buffer, 0, nullptr, &cbData, 0, 0 );
    pDeviceContext->VSSetConstantBuffers( 0, 1, &buffer ); // Actually pass the variables to the vertex shader
    pDeviceContext->DrawIndexedInstanced( mesh.indexCount, instanceCount, 0, 0, 0 );
}

void InstancedRendererEngine2D::RunComputeShader( ID3D11Buffer* buffer, VertexInputData& cbData, int instanceCount, ID3D11ComputeShader* computeShader )
{
    ID3D11ShaderResourceView* shaderResourceViewList[] = { shaderResourceViewA.Get() };
    pDeviceContext->CSSetShaderResources( 0, 1, shaderResourceViewList );

    // Clear hit counts to zero each frame via UAV clears
    constexpr UINT zeros4[4] = { 0, 0, 0, 0 };
    pDeviceContext->ClearUnorderedAccessViewUint( foodCountUAV.Get(), zeros4 );
    pDeviceContext->ClearUnorderedAccessViewUint( nestCountUAV.Get(), zeros4 );

    ID3D11UnorderedAccessView* unorderedAccessViewList[] = { unorderedAccessViewB.Get(), foodCountUAV.Get(), nestCountUAV.Get() };
    UINT initialCounts[]              = { 0, 0, 0 };
    pDeviceContext->CSSetUnorderedAccessViews( 0, 3, unorderedAccessViewList, initialCounts );

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
        size_t n     = game_->state().foodNodes.size();
        for ( size_t i = 0; i < n && i < static_cast<size_t>(MaxFoodNodes); ++i )
        {
            // Sum across bins
            UINT sum = 0;
            for ( int b = 0; b < HitBins; b++ )
                sum += counts[i * HitBins + b];
            if ( sum > 0 )
            {
                auto dec                          = static_cast<float>(sum);
                game_->state().foodNodes[i].amount = max( 0.0f, game_->state().foodNodes[i].amount - dec );
            }
        }
        pDeviceContext->Unmap( foodCountReadback[r].Get(), 0 );
    }

    // Read back nest counts and add to game_->state().score/game_->state().stageScore
    pDeviceContext->CopyResource( nestCountReadback[w].Get(), nestCountBuffer.Get() );
    pDeviceContext->End( nestQuery[w].Get() );
    D3D11_MAPPED_SUBRESOURCE mappedN = {};
    if ( pDeviceContext->GetData( nestQuery[r].Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK &&
         SUCCEEDED( pDeviceContext->Map( nestCountReadback[r].Get(), 0, D3D11_MAP_READ, 0, &mappedN ) ) )
    {
        UINT* countsN  = (UINT*)mappedN.pData;
        UINT totalHits = 0;
        size_t n       = game_->state().foodNodes.size();
        for ( size_t i = 0; i < n && i < static_cast<size_t>(MaxFoodNodes); ++i )
        {
            for ( int b = 0; b < HitBins; b++ )
                totalHits += countsN[i * HitBins + b];
        }
        if ( totalHits > 0 )
        {
            // Combo handling
            if ( game_->state().sinceLastDeposit < 0.5 )
                game_->state().combo = min( 5, game_->state().combo + 1 );
            else
                game_->state().combo = 1;
            game_->state().sinceLastDeposit = 0.0;
            int add                         = static_cast<int>(totalHits) * game_->state().combo;
            game_->state().score += add;
            game_->state().stageScore += add;
        }
        pDeviceContext->Unmap( nestCountReadback[r].Get(), 0 );
    }

    // Swap, back buffer style
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

void InstancedRendererEngine2D::RenderUI()
{
    // Per-node amounts as overlay (no windows)
    if ( ImGui::GetCurrentContext() )
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float base     = game_->state().defaultFoodAmount > 1e-5f ? game_->state().defaultFoodAmount : 100.0f;
        for ( size_t i = 0; i < game_->state().foodNodes.size(); ++i )
        {
            const auto& node = game_->state().foodNodes[i];
            float ratio      = node.amount / base;
            ratio            = max( 0.3f, min( ratio, 2.5f ) );

            // Compute on-screen rect center matching drawn footprint
            Vector2D screen = WorldToScreen( node.pos );
            float sx        = screen.x;
            float sy        = screen.y;
            float widthPx   = ( 0.05f * 2.0f * ratio / aspectRatioX ) * ( static_cast<float>(screenWidth) * 0.5f );
            float heightPx  = ( 0.05f * 2.0f * ratio ) * ( static_cast<float>(screenHeight) * 0.5f );
            ImVec2 center( sx + widthPx * 0.5f, sy + heightPx * 0.5f );

            wchar_t textBuffer[32];
            swprintf_s( textBuffer, L"%.0f", max( 0.0f, node.amount ) );

            char buf[32];
            wcstombs( buf, textBuffer, sizeof( buf ) );

            ImVec2 textSize = ImGui::CalcTextSize( buf );
            ImVec2 pos( center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f );

            // Shadow
            dl->AddText( ImVec2( pos.x + 1, pos.y + 1 ), IM_COL32( 0, 0, 0, 180 ), buf );

            // Text (slightly tinted by active state)
            ImU32 col = ( i == static_cast<size_t>(game_->state().activeFoodIndex) ) ? IM_COL32( 255, 240, 160, 230 )
                                                                        : IM_COL32( 255, 255, 255, 220 );
            dl->AddText( pos, col, buf );
        }
    }

    // HUD window (ImGui) — closable like a debug menu
    if ( game_->state().showDebugHud )
    {
        ImGui::SetNextWindowPos( ImVec2( 20, 20 ), ImGuiCond_Once );
        ImGui::SetNextWindowBgAlpha( 0.6f );
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        if ( ImGui::Begin( "HUD", &game_->state().showDebugHud, flags ) )
        {
            // Keep HUD concise; the main screen handles gameplay stats
            ImGui::Text( "FPS: %.0f", lastFPS );
            ImGui::Separator();
            ImGui::Text( "Debug Controls:" );
            ImGui::BulletText( "F1 or H: toggle HUD" );
            ImGui::BulletText( "F: Frenzy (if off cooldown)" );
        }
        ImGui::End();
    }

    // Overlays (stylized panels instead of windows)
    if ( game_->state().gameState == GameState::StageClear )
    {
        RenderStageClearOverlay();
    }
    else if ( game_->state().gameState == GameState::GameOver )
    {
        RenderOverlay("Game Over","");
    }
}

void InstancedRendererEngine2D::ProcessEvent( UINT msg, WPARAM wParam, LPARAM lParam )
{

    if ( imgui )
        imgui->ProcessWin32Event( msg, wParam, lParam );

    if ( game_ )
    {
        game_->handleEvent( msg, wParam, lParam );
    }

    const bool wantMouse = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;

    switch ( msg )
    {
        case WM_RBUTTONDOWN:
            if ( !wantMouse )
            {
                cameraDragging = true;
                lastMousePos.x = GET_X_LPARAM( lParam );
                lastMousePos.y = GET_Y_LPARAM( lParam );
            }
            break;
        case WM_RBUTTONUP:
            cameraDragging = false;
            break;
        case WM_MOUSEMOVE:
            if ( cameraDragging && !wantMouse )
            {
                POINT current{ GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
                if ( screenWidth > 0 && screenHeight > 0 )
                {
                    float deltaX = ( static_cast<float>(current.x) - static_cast<float>(lastMousePos.x) ) * ( 2.0f / static_cast<float>( screenWidth ) );
                    float deltaY = ( static_cast<float>(current.y) - static_cast<float>(lastMousePos.y) ) * ( 2.0f / static_cast<float>( screenHeight ) );

                    cameraPosition.x -= deltaX;
                    cameraPosition.y += deltaY;

                    cameraPosition.x = std::clamp( cameraPosition.x, -8.0f, 8.0f );
                    cameraPosition.y = std::clamp( cameraPosition.y, -8.0f, 8.0f );
                }
                lastMousePos = current;
            }
            else
            {
                lastMousePos.x = GET_X_LPARAM( lParam );
                lastMousePos.y = GET_Y_LPARAM( lParam );
            }
            break;
        default:
            break;
    }
    // Player placement disabled; sugar/game_->state().hazard spawn randomly now
}

void InstancedRendererEngine2D::RenderProminentStatus()
{
    if ( !ImGui::GetCurrentContext() )
        return;
    int totalSec = static_cast<int>(std::ceil(max(0.0, game_->state().stageTimeLeft)));
    int mm       = totalSec / 60;
    int ss       = totalSec % 60;

    wchar_t w1[64];
    wchar_t w2[64];
    wchar_t w3[64];
    swprintf_s( w1, L"SCORE: %d", game_->state().score );
    swprintf_s( w2, L"TIME LEFT: %02d:%02d", mm, ss );
    swprintf_s( w3, L"TARGET: %d / %d", game_->state().stageScore, game_->state().stageTarget );
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
    // Distinct style: dark slate with a subtle border
    DrawRoundedBackground( dl, p1, p2, IM_COL32( 28, 30, 38, 215 ), IM_COL32( 255, 255, 255, 30 ), 10.0f );

    float x = p1.x + padX;
    float y = p1.y + padY;
    dl->AddText( f, titleSize, ImVec2( x, y ), IM_COL32( 255, 255, 255, 245 ), l1 );
    y += s1.y + gap;
    dl->AddText( f, lineSize, ImVec2( x, y ), IM_COL32( 230, 230, 230, 235 ), l2 );
    y += s2.y + gap;
    dl->AddText( f, lineSize, ImVec2( x, y ), IM_COL32( 230, 230, 230, 235 ), l3 );
}

void InstancedRendererEngine2D::RenderStageClearOverlay()
{
    if ( !ImGui::GetCurrentContext() )
        return;

    const char* title    = "Stage Clear!";
    const char* subtitle = "Choose Upgrade";

    RenderOverlay(title,subtitle);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();
    ImVec2 disp    = ImGui::GetIO().DisplaySize;

    float titleSize      = 28.0f;
    float subtitleSize   = 20.0f;
    float headerSize     = 18.0f; // header above each button
    float btnW           = disp.x / 6.0f;
    float btnH           = 44.0f;
    float gap            = 10.0f;
    float padY           = 16.0f;

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
    ImVec2 pos( disp.x / 2.0f - ( contentW / 2 ), disp.y / 3.0f );

    float textStartY = pos.y + padY;

    // Buttons row (3) with headers
    Button buttons[]     = { { "Speed", "+15%", 1 }, { "Spawn Rate", "+25%", 2 }, { "Max Ants", "+128", 3 } };
    float buttonStartX   = pos.x + ( (contentW)-rowW ) * 0.5f;
    bool clicked         = ImGui::IsMouseClicked( ImGuiMouseButton_Left );
    ImVec2 mousePosition = ImGui::GetIO().MousePos;

    float scaleH = headerSize / ImGui::GetFontSize();
    for (const Button& b : buttons)
    {

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
            game_->applyUpgrade( b.id );
            return;
        }

        buttonStartX += btnW + gap;
    }
}

void InstancedRendererEngine2D::RenderOverlay(const char* overlayTitle, const char* overlaySubtitle)
{
    if ( !ImGui::GetCurrentContext() )
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f      = ImGui::GetFont();
    ImVec2 disp    = ImGui::GetIO().DisplaySize;

    const char* title    = overlayTitle;
    const char* subtitle = overlaySubtitle;
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

    // Center title and subtitle horizontally within a panel
    float titleXPos = pos.x + ( contentW - ts.x ) * 0.5f;
    dl->AddText( f, titleSize, ImVec2( titleXPos, textStartY ), IM_COL32( 255, 255, 255, 245 ), title );
    textStartY += ts.y + 6.0f;

    float subTitleXPos = pos.x + ( (contentW)-ssb.x ) * 0.5f;
    dl->AddText( f, subtitleSize, ImVec2( subTitleXPos, textStartY ), IM_COL32( 230, 230, 230, 240 ), subtitle );
}

int InstancedRendererEngine2D::GetActiveFoodIndex() const
{
    return game_->state().activeFoodIndex;
}
