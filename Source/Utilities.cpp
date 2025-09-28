#include "Utilities.h"

std::vector<char> Utilities::ReadShaderBinary( const wchar_t* filePath )
{
    std::ifstream inputStream( filePath, std::ios::binary | std::ios::ate );

    if ( !inputStream.is_open() )
    {
        throw std::runtime_error( "Failed to load shader binary" );
    }

    std::streamsize fileSize = inputStream.tellg();
    std::vector<char> fileData( fileSize );

    inputStream.seekg( 0, std::ios::beg );

    if ( !inputStream.read( fileData.data(), fileSize ) )
    {
        throw std::runtime_error( "Failed to read shader binary" );
    }

    return fileData;
}

bool Utilities::CreateVertexShader( ID3D11Device* device, HRESULT& hr, const wchar_t* vsFilePath,
                                    ID3D11VertexShader** vertexShader, ID3DBlob** vsBlob )
{

    // Already compiled by msbuild
    std::vector<char> compiledShader = Utilities::ReadShaderBinary( vsFilePath );

    hr = device->CreateVertexShader( compiledShader.data(), compiledShader.size(), nullptr, vertexShader );
    if ( FAILED( hr ) )
    {
        return false;
    }

    // Create a blob for the passed in blob pointer pointer
    hr = D3DCreateBlob( compiledShader.size(), vsBlob );
    if ( SUCCEEDED( hr ) )
    {
        // Using memcpy_s as practice
        memcpy_s( ( *vsBlob )->GetBufferPointer(), compiledShader.size(), compiledShader.data(),
                  compiledShader.size() );
    }

    return true;
}

bool Utilities::CreatePixelShader( ID3D11Device* device, HRESULT& hr, const wchar_t* psFilePath,
                                   ID3D11PixelShader** pixelShader )
{
    // Already compiled by msbuild
    std::vector<char> compiledShader = Utilities::ReadShaderBinary( psFilePath );

    hr = device->CreatePixelShader( compiledShader.data(), compiledShader.size(), nullptr, pixelShader );
    if ( FAILED( hr ) )
    {
        return false;
    }

    return true;
}

bool Utilities::CreateComputeShader( ID3D11Device* device, HRESULT& hr, const wchar_t* csFilePath,
                                     ID3D11ComputeShader** computeShader )
{
    // Already compiled by msbuild
    std::vector<char> compiledShader = Utilities::ReadShaderBinary( csFilePath );

    hr = device->CreateComputeShader( compiledShader.data(), compiledShader.size(), nullptr, computeShader );
    if ( FAILED( hr ) )
    {
        return false;
    }

    return true;
}
