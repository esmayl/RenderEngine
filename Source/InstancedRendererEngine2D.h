#pragma once

#include "BaseRenderer.h"
#include "Block2D.h"
#include "FoodNode.h"
#include "GameEnums.h"
#include "Hazard.h"
#include "ImGuiRenderer.h"
#include "InstanceData.h"
#include "Objects/SquareMesh.h"
#include "Objects/TriangleMesh.h"
#include "Rendering/DrawHelpers.h"
#include "Rendering/QuadRenderer2D.h"
#include "Rendering/RenderStates.h"
#include "Utilities.h"
#include "Vector2D.h"
#include "Vertex.h"
#include "VertexInputData.h"
#include "imgui.h"

#include <Windows.h>
#include <Windowsx.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <d3d11.h>
#include <d3dcompiler.h> // Needed for compiling shaders
#include <deque>
#include <dxgi.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

// (Plain ImGui optional) No backends used

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

    void RenderWavingGrid( int gridWidth, int gridHeight, bool attachToCamera );

    void RenderFlock( int instanceCount );

    void RenderHud();

    void RenderUI();
    void RenderProminentStatus();
    void RenderEventBanner();
    void RenderTopLeftStats();
    void RenderStageClearOverlay();
    void RenderGameOverOverlay();

    void ResetAnts();

    void ProcessEvent( UINT msg, WPARAM wParam, LPARAM lParam )
    {
        if ( imgui )
            imgui->ProcessWin32Event( msg, wParam, lParam );

        const bool wantMouse = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;

        switch ( msg )
        {
        case WM_KEYUP:
            OnKeyUp( (WPARAM)wParam );
            break;
        case WM_LBUTTONUP:
            if ( partyMode )
            {
                int mx = GET_X_LPARAM( lParam );
                int my = GET_Y_LPARAM( lParam );
                TriggerConfettiBurst( mx, my, 90 );
            }
            break;
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
                    float deltaX = ( current.x - lastMousePos.x ) * ( 2.0f / static_cast<float>( screenWidth ) );
                    float deltaY = ( current.y - lastMousePos.y ) * ( 2.0f / static_cast<float>( screenHeight ) );

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
        // Player placement disabled; sugar/hazard spawn randomly now
    }

    void RenderFoodMarkers();

    void RenderFoodLabels();

    int GetActiveFoodIndex() const;

    void SetFlockTarget( int x, int y );

    void SetFood( int x, int y, float amount );

    void SetNest( int x, int y );

    void UpdateGame( double dt );

    // Food system
    void SpawnRandomFood( int count );

    void SpawnFoodAtScreen( int x, int y, float amount );

    int FindNearestFoodScreen( int x, int y, float maxPixelRadius ) const;

    void SetActiveFoodByIndex( int index );

    // Stages & flow
    void ResetGame();

    void StartStage( int number );

    void AdvanceStage();

    void ApplyUpgrade( int option );

    void ToggleEndless( bool enabled );

    void RebuildDepartureStagger();

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
    // Common reusable D3D states
    std::unique_ptr<RenderStates> _states;

    std::vector<InstanceData> instances;
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

    void CreateBuffers();

    void RunComputeShader( ID3D11Buffer* buffer, VertexInputData& cbData, int instanceCount,
                           ID3D11ComputeShader* computeShader );

    void SetupViewport( UINT width, UINT height );

    Vector2D ScreenToWorld( int x, int y ) const;
    Vector2D WorldToScreen( const Vector2D& world ) const;
    Vector2D WorldToView( const Vector2D& world ) const;

    void InitRenderBufferAndTargetView( HRESULT& hr );

    void SetupVerticesAndShaders( UINT& stride, UINT& offset, UINT bufferCount, Mesh* mesh,
                                  ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader );

    void PassInputDataAndRunInstanced( ID3D11Buffer* pConstantBuffer, VertexInputData& cbData, Mesh& mesh,
                                       int instanceCount );

    void RenderMarker( const Vector2D& ndcPos, float size, int colorCode );

    void RenderRectTL( const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode, int lightenLevel );

    void RenderMarkerWithMesh( Mesh* mesh, const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode,
                               int lightenLevel = 0 );

    void RenderPanel( const Vector2D& ndcCenter, float sizeX, float sizeY, int colorCode );

    void RenderUIPanel( int x, int y, int width, int height, float r, float g, float b, float a );

    void RenderRainOverlay();

    // Fun elements
    inline void OnKeyUp( WPARAM vk )
    {
        // Konami Code: Up,Up,Down,Down,Left,Right,Left,Right,B,A
        static const WPARAM seq[] = { VK_UP, VK_UP, VK_DOWN, VK_DOWN, VK_LEFT, VK_RIGHT, VK_LEFT, VK_RIGHT, 'B', 'A' };
        const int seqLen          = (int)( sizeof( seq ) / sizeof( seq[0] ) );
        if ( vk == seq[konamiIndex] )
        {
            konamiIndex++;
            if ( konamiIndex >= seqLen )
            {
                konamiIndex = 0;
                partyMode   = !partyMode;
                TriggerConfettiBurst( (int)( screenWidth * 0.5f ), (int)( screenHeight * 0.5f ), 160 );
            }
        }
        else
        {
            konamiIndex = ( vk == seq[0] ) ? 1 : 0;
        }

        // Frenzy hotkey
        if ( vk == 'F' )
        {
            if ( !frenzyActive && frenzySinceLast >= frenzyCooldown )
            {
                frenzyActive   = true;
                frenzyTimeLeft = 4.0; // seconds
            }
        }

        // Toggle debug HUD window
        if ( vk == VK_F1 || vk == 'H' )
        {
            showDebugHud = !showDebugHud;
        }
    }
    void UpdateParty( double dt );
    void RenderPartyOverlay();
    void TriggerConfettiBurst( int x, int y, int count );

    // Ants-themed extras
    void RenderPheromonePath();
    void SpawnBonusSugarAtScreen( int x, int y, float amount, float mult, float decay );

    Vector2D flockTarget;
    Vector2D previousFlockTarget;
    double flockTransitionTime;
    double flockFrozenTime;

    // Game state
    Vector2D nestPos{ 0.0f, 0.0f };
    std::vector<FoodNode> foodNodes;
    int activeFoodIndex     = -1;
    float minFoodSpacing    = 0.12f; // NDC units, keep nodes separated
    float defaultFoodAmount = 100.0f;
    int depositingFoodIndex = -1;
    int maxAnts             = 512;
    int activeAnts          = 64;
    double spawnAccumulator = 0.0;
    float antsPerSecond     = 32.0f;
    int score               = 0;
    float antSpeed          = 0.5f;  // must match cbData.speed
    float initialSpeed      = 0.5f;  // from settings.ini
    float followDistance    = 0.05f; // must match compute shader
    double legElapsed       = 0.0;
    double travelTime       = 0.0;
    float scoreCarryAccum   = 0.0f;
    AntMode mode            = AntMode::Idle;

    // Spawning queue: delay new ants before activation
    std::deque<double> pendingSpawns;

    // Settings
    int initialAnts      = 10;
    double spawnDelaySec = 0.1;
    bool enableSugar     = true;
    bool enableHazard    = true;

    void LoadSettings();

    // Stage system
    GameState gameState     = GameState::Playing;
    int stage               = 1;
    double stageTimeLeft    = 60.0;
    int stageTarget         = 200; // food units to collect this stage
    int stageScore          = 0;   // collected this stage
    int combo               = 1;   // deposit combo multiplier
    double sinceLastDeposit = 9999.0;
    bool endlessMode        = false;
    bool upgradePending     = false;

    // Random slow event (e.g., rain)
    bool slowActive      = false;
    double slowTimeLeft  = 0.0;
    double slowCooldown  = 8.0;
    double slowSinceLast = 0.0;

    // Hazard (repellent) — inactive and stationary until user places it
    Hazard hazard{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.04f, false };
    double hazardTimeLeft  = 0.0;  // seconds remaining when active
    double hazardDuration  = 5.0;  // duration for random hazard
    double hazardSinceLast = 0.0;  // time since last random hazard
    double hazardCooldown  = 12.0; // seconds between random hazards

    // ImGui wrapper
    std::unique_ptr<ImGuiRenderer> imgui;

    // --- Fun elements state ---
    struct PartyParticle
    {
        float x, y;     // pixels
        float vx, vy;   // pixels/sec
        float life;     // remaining seconds
        float ttl;      // total lifetime seconds
        uint32_t color; // IM_COL32 RGBA
        int shape;      // 0: rect, 1: tri, 2: circle
        float size;     // pixels
        float rot;      // radians
        float rotVel;   // radians/sec
    };
    std::vector<PartyParticle> partyParticles;
    bool partyMode           = false;
    bool stageClearBurstDone = false;
    int konamiIndex          = 0;

    // --- Ants attacks gameplay ---
    bool frenzyActive      = false;
    double frenzyTimeLeft  = 0.0;
    double frenzyCooldown  = 12.0;
    double frenzySinceLast = 0.0;
    // Debug staging toggles
    bool antsEnabled  = true;  // start staged: enable after markers are validated
    bool showDebugHud = false; // closable debug HUD window (F1/H)

    // Placement disabled; random events only

    // Bonus sugar spawner
    double bonusSpawnSince    = 0.0;
    double bonusSpawnInterval = 10.0; // seconds between random sugar spawns
};
