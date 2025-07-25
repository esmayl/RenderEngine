#pragma once
#include "Mesh.h";
#include "Vertex.h";

class TriangleMesh
{
public:
	Mesh* renderingData;
	explicit TriangleMesh(ID3D11Device& pDevice);
};
