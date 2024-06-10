#pragma once

#include "DXHelpers.h"

namespace dxpg
{
enum class ShaderType {
	Vertex,
	Pixel,
	Compute
};
struct Shader
{
	ComPtr<ID3DBlob> Blob;
	
	std::wstring EntryPoint;
	std::wstring Name;
	std::wstring Path;
	ShaderType Type;
};
}