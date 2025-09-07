#include "SquareMesh.h"

SquareMesh::SquareMesh( ID3D11Device& pDevice )
{
    renderingData = new Mesh();

    Vertex vertices[] = {
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // Top-left
        { 0.05f, 0.0f, 0.0f, 1.0f, 0.0f },  // Top-right
        { 0.0f, -0.05f, 0.0f, 0.0f, 1.0f }, // Bottom-left
        { 0.05f, -0.05f, 0.0f, 1.0f, 1.0f } // Bottom-right
    };

    // Create triangle indices
    UINT indices[] = {
        0, 1, 2, // first triangle
        2, 1, 3  // second triangle
    };
    renderingData->indexCount = std::size( indices );

    // Create buffer description for vertices
    D3D11_BUFFER_DESC bd      = {};
    D3D11_SUBRESOURCE_DATA sd = {};

    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( vertices );
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    sd.pSysMem = vertices;

    HRESULT hr = pDevice.CreateBuffer( &bd, &sd, &renderingData->vertexBuffer );
    if ( FAILED( hr ) )
        return;

    // Re-use buffer description for the indices
    bd.ByteWidth = sizeof( indices );
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    sd.pSysMem = indices;

    hr = pDevice.CreateBuffer( &bd, &sd, &renderingData->indexBuffer );
    if ( FAILED( hr ) )
        return;
}
