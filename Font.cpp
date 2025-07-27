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

        int x,y;

        pChar->QueryIntAttribute("id",&fontDescription.id);
        pChar->QueryIntAttribute("x", &fontDescription.x);
        pChar->QueryIntAttribute("y", &fontDescription.y);
        pChar->QueryIntAttribute("width", &fontDescription.id);
        pChar->QueryIntAttribute("height", &fontDescription.id);
        pChar->QueryIntAttribute("xoffset", &x);
        pChar->QueryIntAttribute("yoffset", &y);

        fontDescription.x += x; // Just add / subtract the offset to make it easier to work with
        fontDescription.y += y; // Just add / subtract the offset to make it easier to work with

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
