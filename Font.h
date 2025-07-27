#pragma once
#include <string>
#include <map>
#include "FontCharDescription.h"
#include "Vector2D.h"
#include <tinyxml2.h>
#include <stdexcept>

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
};

