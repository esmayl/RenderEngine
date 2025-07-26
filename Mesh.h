#pragma once
#include <d3d11.h>
#include <iterator> // Required for std::size
#include "UV.h"

#pragma comment(lib, "d3d11.lib")

struct Mesh
{
	ID3D11Buffer* vertexBuffer = nullptr;
	ID3D11Buffer* indexBuffer = nullptr;
    UV uv;
	UINT indexCount = 0;

    ~Mesh()
    {
        if(vertexBuffer) vertexBuffer->Release();
        if(indexBuffer) indexBuffer->Release();
    }
};