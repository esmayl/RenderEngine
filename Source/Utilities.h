#pragma once
#include "Block2D.h"
#include "RandomGenerator.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h> // Needed for compiling shaders
#include <dxgi.h>
#include <fstream>
#include <vector>

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )

class Utilities
{
  public:
    static std::vector<Block2D> CreateBlocks( int totalWidth, int totalHeight, int columns, int rows );
    static void CustomDrawText( HDC buffer, const wchar_t textToDraw[] );
    static std::vector<char> ReadShaderBinary( const wchar_t* filePath );
    static bool CreateVertexShader( ID3D11Device* device, HRESULT& hr, const wchar_t* vsFilePath,
                                    ID3D11VertexShader** vertexShader, ID3DBlob** vsBlob );
    static bool CreatePixelShader( ID3D11Device* device, HRESULT& hr, const wchar_t* psFilePath,
                                   ID3D11PixelShader** pixelShader );
    static bool CreateComputeShader( ID3D11Device* device, HRESULT& hr, const wchar_t* csFilePath,
                                     ID3D11ComputeShader** computeShader );
};
