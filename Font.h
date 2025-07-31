#pragma once
#include <string>
#include <map>
#include <vector>
#include <tinyxml2.h>
#include <stdexcept>
#include <wincodec.h>
#include <d3d11.h>

#include "FontCharDescription.h"
#include "Vector2D.h"

#pragma comment(lib, "d3d11.lib")

class Font
{

	public:
		void LoadFonts(const char* newTextureFileName);
		FontCharDescription GetFontCharacter(char character);
		
		int GetCharacterSize();
		int GetCharacterCount();
		int GetLineHeight();
		
		bool IsBold();
		bool IsItalic();
		
		Vector2D GetTextureSize();
		ID3D11ShaderResourceView* GetTexture();
		void LoadTexture(ID3D11Device* pDevice);

		~Font();
	private:

		std::string fontName;
		
		bool bold;
		bool italic;		
		
		int characterSize;
		int characterCount;
		int lineHeight;
		int textureWidth;
		int textureHeight;
		
		std::string textureFileName;
		std::map<char,FontCharDescription> fontCharacters;
		ID3D11ShaderResourceView* m_pTextureView = nullptr;
};
