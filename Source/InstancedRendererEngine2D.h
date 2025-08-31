#pragma once

#include <chrono>
#include <vector>
#include <deque>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h> // Needed for compiling shaders

#include "Utilities.h"
#include "Block2D.h"
#include "BaseRenderer.h"
#include "Vertex.h"
#include "VertexInputData.h"
#include "TextInputData.h"
#include "InstanceData.h"
#include "Objects/SquareMesh.h"
#include "Objects/TriangleMesh.h"
#include "Font.h"
#include "Vector2D.h"
#include "GameEnums.h"
#include "FoodNode.h"
#include "Hazard.h"
#include "ImGuiRenderer.h"

// (Plain ImGui optional) No backends used

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class InstancedRendererEngine2D : public BaseRenderer
{
	public:
		void Init(HWND windowHandle, int blockWidth, int blockHeight) override;

		void OnPaint(HWND windowHandle) override;
		void OnResize(int width, int height) override;
		void CountFps() override;
		void OnShutdown() override;
		void RenderWavingGrid(int gridWidth, int gridHeight);
		void RenderFlock(int instanceCount);
			void RenderFpsText(int xPos, int yPos, int fontSize);
			void RenderHud();
			void RenderText(int xPos, int yPos, int fontSize, const wchar_t* text);
			void RenderUI();
            void ResetAnts();
            void ProcessEvent(UINT msg, WPARAM wParam, LPARAM lParam) { if (imgui) imgui->ProcessWin32Event(msg,wParam,lParam); }
            void RenderFoodMarkers();
            void RenderFoodLabels();
            int GetActiveFoodIndex() const { return activeFoodIndex; }

		void SetFlockTarget(int x, int y);
		void SetFood(int x, int y, float amount);
			void SetNest(int x, int y);
			void UpdateGame(double dt);

			// Food system
			void SpawnRandomFood(int count);
			void SpawnFoodAtScreen(int x, int y, float amount);
			int  FindNearestFoodScreen(int x, int y, float maxPixelRadius) const;
			void SetActiveFoodByIndex(int index);

            // Stages & flow
            void ResetGame();
            void StartStage(int number);
            void AdvanceStage();
            void ApplyUpgrade(int option);
            void ToggleEndless(bool enabled);
            void RebuildDepartureStagger();

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		
		double deltaTime = 0;
			double timeSinceFPSUpdate = 0.0;
			int framesSinceFPSUpdate = 0;
			double totalTime = 0.0f;
			double lastFPS = 0.0;

		wchar_t fpsText[256];

		ID3D11Device* pDevice;
		ID3D11DeviceContext* pDeviceContext;
		IDXGISwapChain* pSwapChain;
		ID3D11RenderTargetView* renderTargetView;
		ID3D11Texture2D* backBuffer;

		// for rendering
		ID3D11Buffer* pConstantBuffer;
		ID3D11Buffer* flockConstantBuffer;
		ID3D11VertexShader* waveVertexShader;
            ID3D11VertexShader* textVertexShader;
            ID3D11VertexShader* flockVertexShader;
            ID3D11VertexShader* colorVertexShader;
            ID3D11VertexShader* uiVertexShader;
            ID3D11PixelShader* plainPixelShader;
            ID3D11PixelShader* textPixelShader;
            ID3D11ComputeShader* flockComputeShader;
            ID3D11InputLayout* pInputLayout; // Used to define input variables for the shaders
            ID3D11InputLayout* flockInputLayout; // Used to define input variables for the shaders
            ID3D11RasterizerState* scissorRasterizer = nullptr; // For rain overlay clipping
            ID3D11BlendState* blendState = nullptr; // Persistent alpha blend state

		std::vector<InstanceData> instances;
		ID3D11Buffer* instanceBuffer;
		ID3D11SamplerState* textureSamplerState;

		// Compute shader buffers
		ID3D11ShaderResourceView* shaderResourceViewA = nullptr;
		ID3D11ShaderResourceView* shaderResourceViewB = nullptr;
		ID3D11UnorderedAccessView* unorderedAccessViewA = nullptr;
            ID3D11UnorderedAccessView* unorderedAccessViewB = nullptr;
            ID3D11Buffer* computeBufferA = nullptr;
            ID3D11Buffer* computeBufferB = nullptr;
            // Food/nest hit counts (GPU) with simple binning to reduce atomics contention
            static const int MaxFoodNodes = 128;
            static const int HitBins = 8;
            ID3D11Buffer* foodCountBuffer = nullptr;
            ID3D11UnorderedAccessView* foodCountUAV = nullptr;
            ID3D11Buffer* foodCountReadback[3] = {nullptr,nullptr,nullptr};
            ID3D11Query* foodQuery[3] = {nullptr,nullptr,nullptr};
            // Nest hit counts (GPU)
            ID3D11Buffer* nestCountBuffer = nullptr;
            ID3D11UnorderedAccessView* nestCountUAV = nullptr;
            ID3D11Buffer* nestCountReadback[3] = {nullptr,nullptr,nullptr};
            ID3D11Query* nestQuery[3] = {nullptr,nullptr,nullptr};
            int readbackCursor = 0;

		UINT screenWidth;
		UINT screenHeight;
		float aspectRatioX;

		SquareMesh* square = nullptr;
		TriangleMesh* triangle = nullptr;
		Font* font = nullptr;

		void LoadShaders();
		void CreateMeshes();
		void CreateBuffers();
		void RunComputeShader(ID3D11Buffer* buffer, VertexInputData& cbData, int instanceCount, ID3D11ComputeShader* computeShader);
		void SetupViewport(UINT width, UINT height);
		void InitRenderBufferAndTargetView(HRESULT& hr);
		void SetupVerticesAndShaders(UINT& stride, UINT& offset, UINT bufferCount, Mesh* mesh, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader);
            void PassInputDataAndRunInstanced(ID3D11Buffer* pConstantBuffer, VertexInputData& cbData, Mesh& mesh, int instanceCount);
            void RenderMarker(const Vector2D& ndcPos, float size, int colorCode);
            void RenderRectTL(const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode, int lightenLevel);
            void RenderMarkerWithMesh(Mesh* mesh, const Vector2D& ndcTopLeft, float sizeX, float sizeY, int colorCode, int lightenLevel=0);
            void RenderPanel(const Vector2D& ndcCenter, float sizeX, float sizeY, int colorCode);
            void RenderUIPanel(int x, int y, int width, int height, float r, float g, float b, float a);
            void RenderRainOverlay();

		Vector2D flockTarget;
		Vector2D previousFlockTarget;
		double flockTransitionTime;
		double flockFrozenTime;

            // Game state
            Vector2D nestPos{0.0f, 0.0f};
            std::vector<FoodNode> foodNodes;
            int activeFoodIndex = -1;
            float minFoodSpacing = 0.12f; // NDC units, keep nodes separated
            float defaultFoodAmount = 100.0f;
            int depositingFoodIndex = -1;
            int maxAnts = 512;
            int activeAnts = 64;
            double spawnAccumulator = 0.0;
            float antsPerSecond = 32.0f;
            int score = 0;
            float antSpeed = 0.5f; // must match cbData.speed
            float initialSpeed = 0.5f; // from settings.ini
            float followDistance = 0.05f; // must match compute shader
            double legElapsed = 0.0;
            double travelTime = 0.0;
            float scoreCarryAccum = 0.0f;
            AntMode mode = AntMode::Idle;

            // Spawning queue: delay new ants before activation
            std::deque<double> pendingSpawns;

            // Settings
            int initialAnts = 10;
            double spawnDelaySec = 0.1;
            void LoadSettings();

            // Stage system
            GameState gameState = GameState::Playing;
            int stage = 1;
            double stageTimeLeft = 60.0;
            int stageTarget = 200;     // food units to collect this stage
            int stageScore = 0;        // collected this stage
            int combo = 1;             // deposit combo multiplier
            double sinceLastDeposit = 9999.0;
            bool endlessMode = false;
            bool upgradePending = false;

            // Random slow event (e.g., rain)
            bool slowActive = false;
            double slowTimeLeft = 0.0;
            double slowCooldown = 8.0;
            double slowSinceLast = 0.0;

            // Hazard
            Hazard hazard{ {0.3f, 0.0f}, {0.15f, 0.12f}, 0.2f, true };

            // ImGui wrapper
            ImGuiRenderer* imgui = nullptr;
};
