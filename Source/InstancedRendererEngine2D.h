#pragma once

#include "Objects/SquareMesh.h"
#include "Objects/TriangleMesh.h"
#include "Game/AntGame.h"
#include "BaseRenderer.h"
#include "ImGuiRenderer.h"
#include "InstanceData.h"
#include "Vector2D.h"
#include "VertexInputData.h"
#include "Utilities.h"
#include "Button.h"

#include <algorithm>
#include <Windowsx.h> // Required for GET_X_LPARAM and GET_Y_LPARAM
#include <array>
#include <chrono>
#include <d3d11.h>
#include <dxgi.h>
#include <memory>
#include <vector>
#include <wrl/client.h>


#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "user32.lib" )


class InstancedRendererEngine2D : public BaseRenderer
{
  public:
    void Init( HWND windowHandle, int blockWidth, int blockHeight ) override;

    void OnPaint( HWND windowHandle ) override;

    void OnResize( int width, int height ) override;

    void CountFps() override;

    void OnShutdown() override;

    void ProcessEvent( UINT msg, WPARAM wParam, LPARAM lParam );

    void SetGame( Game::AntGame* game );

    double GetDeltaTime() const
    {
        return deltaTime;
    }
    double GetTotalTime() const
    {
        return totalTime;
    }
    int GetScreenWidth() const
    {
        return static_cast<int>( screenWidth );
    }
    int GetScreenHeight() const
    {
        return static_cast<int>( screenHeight );
    }

    void InitializeSimulationBuffers( const std::vector<InstanceData>& instances );
    void UploadInstanceBuffer( const std::vector<InstanceData>& instances );
    void UploadInstanceSlot( int slot, const InstanceData& data ) const;
    void ResizeInstanceStorage( const std::vector<InstanceData>& instances );

    Vector2D ScreenToWorld( int x, int y ) const;
    Vector2D WorldToScreen( const Vector2D& world ) const;
    Vector2D WorldToView( const Vector2D& world ) const;

  private:
    std::chrono::time_point<std::chrono::steady_clock> startTime;

    double deltaTime          = 0;
    double timeSinceFPSUpdate = 0.0;
    int framesSinceFPSUpdate  = 0;
    double totalTime          = 0.0f;
    double lastFPS            = 0.0;

    wchar_t fpsText[30];

    Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> pSwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

    // for rendering
    Microsoft::WRL::ComPtr<ID3D11Buffer> pConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> flockConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> waveVertexShader;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> flockVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> colorVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> uiVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> plainPixelShader;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> flockComputeShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> pInputLayout;          // Used to define input variables for the shaders
    Microsoft::WRL::ComPtr<ID3D11InputLayout> flockInputLayout;      // Used to define input variables for the shaders
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> scissorRasterizer; // For rain overlay clipping

    Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;

    // Compute shader buffers
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceViewA;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceViewB;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unorderedAccessViewA;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unorderedAccessViewB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> computeBufferA;
    Microsoft::WRL::ComPtr<ID3D11Buffer> computeBufferB;
    // Food/nest hit counts (GPU) with simple binning to reduce atomics contention
    static const int MaxFoodNodes = 128;
    static const int HitBins      = 8;
    Microsoft::WRL::ComPtr<ID3D11Buffer> foodCountBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> foodCountUAV;
    std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 3> foodCountReadback{};
    std::array<Microsoft::WRL::ComPtr<ID3D11Query>, 3> foodQuery{};
    // Nest hit counts (GPU)
    Microsoft::WRL::ComPtr<ID3D11Buffer> nestCountBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> nestCountUAV;
    std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 3> nestCountReadback{};
    std::array<Microsoft::WRL::ComPtr<ID3D11Query>, 3> nestQuery{};
    int readbackCursor = 0;

    UINT screenWidth;
    UINT screenHeight;
    float aspectRatioX;

    Vector2D cameraPosition{};
    bool cameraDragging = false;
    POINT lastMousePos{};
    float cameraZoom = 1.0f;

    std::unique_ptr<SquareMesh> square;
    std::unique_ptr<TriangleMesh> triangle;

    void LoadShaders();

    void CreateMeshes();

    void CreateBuffers( const std::vector<InstanceData>& instances );

    void RunComputeShader( ID3D11Buffer* buffer, VertexInputData& cbData, int instanceCount, ID3D11ComputeShader* computeShader );

    void SetupViewport( UINT width, UINT height );

    void InitRenderBufferAndTargetView( HRESULT& hr );

    void SetupVerticesAndShaders( const UINT& stride, const UINT& offset, const Mesh* mesh, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader ) const;

    void PassInputDataAndRunInstanced( ID3D11Buffer* pConstantBuffer, VertexInputData& cbData, Mesh& mesh, int instanceCount ) const;
    void RenderUI();

    void RenderProminentStatus();

    void RenderStageClearOverlay();

    void RenderOverlay(const char* overlayTitle, const char* overlaySubtitle);

    int GetActiveFoodIndex() const;

    Game::AntGame* game_ = nullptr;

    std::unique_ptr<ImGuiRenderer> imgui;
};
