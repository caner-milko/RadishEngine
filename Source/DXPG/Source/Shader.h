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

	std::unordered_map<std::string, D3D12_SIGNATURE_PARAMETER_DESC> InputParameters;
	std::unordered_map<std::string, D3D12_SHADER_INPUT_BIND_DESC> ResourceBindings;
};
}