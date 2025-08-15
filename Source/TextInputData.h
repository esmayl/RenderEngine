#pragma once

#include "DirectXMath.h"

struct TextInputData
{
	DirectX::XMFLOAT2 size; // 8 bytes
	DirectX::XMFLOAT2 objectPos; // Range 0 to 1 starting left
	DirectX::XMFLOAT2 screenSize;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT2 uvScale;
	float padding;
	float padding2;
};