#pragma once

#include <chrono>
#include <vector>
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

		void SetFlockTarget(int x, int y);

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		
		double deltaTime = 0;
		double timeSinceFPSUpdate = 0.0;
		int framesSinceFPSUpdate = 0;
		double totalTime = 0.0f;

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
		ID3D11PixelShader* plainPixelShader;
		ID3D11PixelShader* textPixelShader;
		ID3D11ComputeShader* flockComputeShader;
		ID3D11InputLayout* pInputLayout; // Used to define input variables for the shaders
		ID3D11InputLayout* flockInputLayout; // Used to define input variables for the shaders

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

		Vector2D flockTarget;
		Vector2D previousFlockTarget;
		double flockTransitionTime;
		double flockFrozenTime;
};