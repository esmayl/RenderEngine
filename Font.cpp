#include "Font.h"

void Font::LoadFonts(const char* fontFileName)
{
    tinyxml2::XMLDocument fontFile;
    if(fontFile.LoadFile(fontFileName) != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error("Failed to load font");
    }

    // Get the root <font> element.
    tinyxml2::XMLElement* font = fontFile.FirstChildElement("font");
    if(!font) 
    {
        throw std::runtime_error("Font xml has no root element of 'font'");
    }

    // Get the <common> element.
    tinyxml2::XMLElement* fontInfo = font->FirstChildElement("info");
    if(!fontInfo)
    {
        throw std::runtime_error("Font xml has no root element of 'info'");
    }

    const char* newFontName = nullptr;
    fontInfo->QueryStringAttribute("face", &newFontName);
    fontName = std::string(newFontName);
    
    fontInfo->QueryIntAttribute("size", &characterSize);
    fontInfo->QueryBoolAttribute("bold", &bold);
    fontInfo->QueryBoolAttribute("italic", &italic);

    // Get the <common> element.
    tinyxml2::XMLElement* fontCommon = font->FirstChildElement("common");
    if(!fontCommon)
    {
        throw std::runtime_error("Font xml has no root element of 'common'");
    }

    fontCommon->QueryIntAttribute("lineHeight",&lineHeight);
    fontCommon->QueryIntAttribute("scaleW", &textureWidth);
    fontCommon->QueryIntAttribute("scaleH", &textureHeight);

    // Get the <common> element.
    tinyxml2::XMLElement* fontPages = font->FirstChildElement("pages");
    if(!fontPages)
    {
        throw std::runtime_error("Font xml has no root element of 'common'");
    }

    tinyxml2::XMLElement* texturePage = fontPages->FirstChildElement("page");
    if(!texturePage)
    {
        throw std::runtime_error("Font xml has no root element of 'common'");
    }

    const char* newFileName = nullptr;
    texturePage->QueryStringAttribute("file",&newFileName);
    textureFileName = std::string(newFileName);    


    // Get the <chars> element.
    tinyxml2::XMLElement* pChars = font->FirstChildElement("chars");
    if(!pChars)
    {
        throw std::runtime_error("Font xml has no element of 'chars'");
    }

    pChars->QueryIntAttribute("count", &characterCount);

    // Get the first <char> element.
    tinyxml2::XMLElement* pChar = pChars->FirstChildElement("char");
    if(!pChar)
    {
        throw std::runtime_error("Font xml has no elements of 'char'");
    }

    while(pChar != nullptr)
    {
        FontCharDescription fontDescription;

        pChar->QueryIntAttribute("id",&fontDescription.id);
        pChar->QueryIntAttribute("x", &fontDescription.x);
        pChar->QueryIntAttribute("y", &fontDescription.y);
        pChar->QueryIntAttribute("width", &fontDescription.width);
        pChar->QueryIntAttribute("height", &fontDescription.height);

        fontCharacters[fontDescription.id] = fontDescription;

        pChar = pChar->NextSiblingElement("char"); // Move to the next char element
    }

}

FontCharDescription Font::GetFontCharacter(char character)
{
    int asciiValue = character;
    return fontCharacters[asciiValue];
}

int Font::GetCharacterSize()
{
    return characterSize;
}

int Font::GetCharacterCount()
{
    return characterCount;
}

int Font::GetLineHeight()
{
    return lineHeight;
}

bool Font::IsBold()
{
    return bold;
}

bool Font::IsItalic()
{
    return italic;
}

Vector2D Font::GetTextureSize()
{
    return Vector2D(textureWidth,textureHeight);
}

ID3D11ShaderResourceView* Font::GetTexture()
{
    return m_pTextureView;
}

Font::~Font()
{
    if(m_pTextureView) m_pTextureView->Release();
}

void Font::LoadTexture(ID3D11Device* pDevice)
{
    // Initialize COM
    HRESULT hr = CoInitialize(NULL);

    if(FAILED(hr))
    {
        throw std::runtime_error("Cannot CoInitialize!");
    }

    // The factory pointer
    IWICImagingFactory* pFactory = NULL;

    // Create the COM imaging factory
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory)
    );

    // Create a decoder
    IWICBitmapDecoder* pDecoder = NULL;

    std::wstring wideFileName(textureFileName.begin(), textureFileName.end());

    hr = pFactory->CreateDecoderFromFilename(
        wideFileName.c_str(),                      // Image to be decoded
        NULL,                            // Do not prefer a particular vendor
        GENERIC_READ,                    // Desired read access to the file
        WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
        &pDecoder                        // Pointer to the decoder
    );

    // Retrieve the first frame of the image from the decoder
    IWICBitmapFrameDecode* pFrame = NULL;

    if(SUCCEEDED(hr))
    {
        hr = pDecoder->GetFrame(0, &pFrame);

        IWICFormatConverter* converter = nullptr;
        hr = pFactory->CreateFormatConverter(&converter);
        hr = converter->Initialize(
            pFrame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom
        );
        if(FAILED(hr)) throw std::runtime_error("Failed format convert");

        UINT w, h;
        pFrame->GetSize(&w, &h);

        UINT stride = w * 4; // 4 bytes per pixel (RGBA)
        UINT bufferSize = stride * h;
        std::vector<BYTE> pixels(bufferSize);

     
        converter->CopyPixels(nullptr,
            stride,
            bufferSize,
            pixels.data()
        );

        // Create the texture description.
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // Define the subresource data, pointing to the raw pixel data in your vector.
        D3D11_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pSysMem = pixels.data();
        subresourceData.SysMemPitch = stride; // Bytes per row

        // Actually create the texture
        ID3D11Texture2D* pTexture = nullptr;
        hr = pDevice->CreateTexture2D(&desc, &subresourceData, &pTexture);
        if(FAILED(hr)) throw std::runtime_error("Failed to create D3D11 texture");

        if(pTexture)
        {
            // Assign the created texture to a textureview to be used to pass to gpu
            hr = pDevice->CreateShaderResourceView(pTexture, nullptr, &m_pTextureView);
            pTexture->Release();
            if(FAILED(hr)) throw std::runtime_error("Failed to create font texture view");
        }

        // --- Cleanup WIC objects ---
        converter->Release();

        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        CoUninitialize();
    }
}
