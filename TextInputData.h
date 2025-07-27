#pragma once

#include "DirectXMath.h"

struct TextInputData
{
	DirectX::XMFLOAT2 size; // 8 bytes
	DirectX::XMFLOAT2 objectPos; // Range 0 to 1 starting left
	DirectX::XMFLOAT2 screenSize;
	float padding1;
	float padding2;
};