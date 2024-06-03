#pragma once

#include "DXHelpers.h"
#include "Shader.h"

#include <dxcapi.h>
#include <d3d12shader.h>

namespace dxpg::dx12
{

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