#pragma once

#include "DXHelpers.h"

namespace dxpg::dx12
{
struct Shader
{
	ComPtr<ID3DBlob> Blob;
	std::wstring EntryPoint;
	std::wstring Name;
	std::wstring Path;
};
}