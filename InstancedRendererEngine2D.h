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
#include "InstanceData.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class InstancedRendererEngine2D : public BaseRenderer
{
	public:
		void Init(HWND windowHandle, int blockWidth, int blockHeight) override;
		void SetupViewport(UINT width, UINT height);
		void InitRenderBufferAndTargetView(HRESULT& hr, bool& retFlag);
		void OnPaint(HWND windowHandle) override;
		void OnResize(int width, int height) override;

		void CountFps() override;
		void OnShutdown() override;

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		std::chrono::time_point<std::chrono::steady_clock> lastFrameTime;
		double deltaTime = 0;
		double timeSinceFPSUpdate = 0.0;
		int framesSinceFPSUpdate = 0;
		wchar_t fpsText[20];
		float aspectRatioX;

		std::vector<Block2D> blocks;

		ID3D11Device* pDevice;
		ID3D11DeviceContext* pDeviceContext;
		IDXGISwapChain* pSwapChain;
		ID3D11RenderTargetView* renderTargetView;
		ID3D11Texture2D* pBackBuffer; // Maybe not needed?

		// for rendering triangles
		ID3D11Buffer* pVertexBuffer;
		ID3D11Buffer* pIndexBuffer;
		ID3D11Buffer* pConstantBuffer;
		ID3D11Buffer* pInstanceBuffer;
		ID3D11VertexShader* pVertexShader;
		ID3D11PixelShader* pPixelShader;
		ID3D11InputLayout* pInputLayout; // Used to define input variables for the shaders
		std::vector<InstanceData> instances;

		int columns = 500;
		int rows = 500;

		void CreateShaders();
		void CreateBuffers();
};