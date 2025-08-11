#pragma once

#include <chrono>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h> // Needed for compiling shaders
#include <fstream>

#include "Utilities.h"
#include "Block2D.h"
#include "BaseRenderer.h"
#include "Vertex.h"
#include "VertexInputData.h"
#include "TextInputData.h"
#include "InstanceData.h"
#include "SquareMesh.h"
#include "TriangleMesh.h"
#include "Font.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class InstancedRendererEngine2D : public BaseRenderer
{
	public:
		void Init(HWND windowHandle, int blockWidth, int blockHeight) override;
		void SetupViewport(UINT width, UINT height);
		void InitRenderBufferAndTargetView(HRESULT& hr);
		void OnPaint(HWND windowHandle) override;
		void OnResize(int width, int height) override;

		void RenderWavingGrid(int gridWidth, int gridHeight);
		void RenderFlock(int instanceCount);
		void RenderFpsText(int xPos, int yPos, int fontSize);
		void CountFps() override;
		void OnShutdown() override;

		void SetFlockTarget(float x,float y);

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		double deltaTime = 0;
		double timeSinceFPSUpdate = 0.0;
		int framesSinceFPSUpdate = 0;
		wchar_t fpsText[256];

		std::vector<Block2D> blocks;

		ID3D11Device* pDevice;
		ID3D11DeviceContext* pDeviceContext;
		IDXGISwapChain* pSwapChain;
		ID3D11RenderTargetView* renderTargetView;
		ID3D11Texture2D* pBackBuffer;

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

		float totalTime = 0.0f;
		UINT width;
		UINT height;
		float aspectRatioX;

		SquareMesh* square = nullptr;
		TriangleMesh* triangle = nullptr;
		Font* font = nullptr;

		void CreateShaders();
		bool CreateVertexShader(HRESULT& hr, const wchar_t* vsFilePath, ID3D11VertexShader** vertexShader, ID3DBlob** vsBlob);
		bool CreatePixelShader(HRESULT& hr, const wchar_t* psFilePath, ID3D11PixelShader** pixelShader);
		bool CreateComputeShader(HRESULT& hr, const wchar_t* csFilePath, ID3D11ComputeShader** computeShader);
		void CreateBuffers();
		void CreateFonts(ID3D11Device* device);
		void CreateInstanceList();
		std::vector<char> ReadShaderBinary(const wchar_t* filePath);

		Vector2D flockTarget;
		Vector2D previousFlockTarget;
		float flockTransitionTime;
		float flockFrozenTime;
};