#include "Utilities.h"

std::vector<Block2D> Utilities::CreateBlocks( int totalWidth, int totalHeight, int columns, int rows )
{
    std::vector<Block2D> tempBlocks;
    if ( columns > 0 && rows > 0 )
    {
        tempBlocks.reserve( static_cast<size_t>( columns ) * static_cast<size_t>( rows ) );
    }

    // Prevent division by zero if columns or rows are 0
    if ( columns == 0 || rows == 0 )
    {
        return tempBlocks;
    }

    // Loop through the desired number of columns and rows
    for ( int i = 0; i < columns; i++ )
    {
        for ( int j = 0; j < rows; j++ )
        {
            // Calculate the center position of the block
            int centerX = i;
            int centerY = j;

            // Create the block with the calculated position and size
            tempBlocks.emplace_back( centerX, centerY, 0, 0, RandomGenerator::Generate( 1.0f, 2.0f ) );
        }
    }

    return tempBlocks;
}

void Utilities::CustomDrawText( HDC buffer, const wchar_t textToDraw[] )
{
    // Set the text color (COLORREF is a macro for RGB)
    SetTextColor( buffer, RGB( 0, 0, 0 ) ); // Black text

    // Set the background mode to transparent
    // This prevents the text from having a solid colored box behind it.
    SetBkMode( buffer, TRANSPARENT );

    // Define the rectangle where the text will be drawn
    RECT textRect;
    SetRect( &textRect, 10, 10, 300, 50 ); // A box at top-left: (left, top, right, bottom)

    // Draw the text
    // DT_SINGLELINE: Treats it as one line.
    // DT_LEFT: Aligns text to the left of the rectangle.
    DrawText( buffer, textToDraw, -1, &textRect, DT_SINGLELINE | DT_LEFT );
}

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
