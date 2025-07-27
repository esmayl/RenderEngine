#pragma once

#include "Mesh.h";
#include "Vertex.h";

class SquareMesh
{
	public:
		Mesh* renderingData;
		explicit SquareMesh(ID3D11Device& pDevice);
};

