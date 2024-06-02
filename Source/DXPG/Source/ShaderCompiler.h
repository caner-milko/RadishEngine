#pragma once

#include "DXHelpers.h"

#include <dxcapi.h>
#include <d3d12shader.h>


namespace dxpg::dx12
{

struct Shader
{
	ComPtr<ID3D12ShaderReflection> Reflection;
	ComPtr<ID3DBlob> Blob;
	std::wstring EntryPoint;
	std::wstring Name;
	std::wstring Path;
};

struct ShaderCompiler
{
	static std::unique_ptr<ShaderCompiler> Create();

	enum class ShaderType {
		Vertex,
		Pixel,
		Compute
	};

	std::unique_ptr<Shader> CompileShader(std::wstring_view name, std::wstring_view shaderPath, ShaderType type, std::wstring_view entryPoint = L"main", std::span<std::wstring_view> includeFolders = {});

private:
	ShaderCompiler() = default;
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcCompiler3> Compiler;
	ComPtr<IDxcIncludeHandler> IncludeHandler;
};

}