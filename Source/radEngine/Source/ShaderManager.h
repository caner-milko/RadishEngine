#pragma once

#include "DXHelpers.h"
#include "Shader.h"
#include "RadishCommon.h"

#include <dxcapi.h>
#include <d3d12shader.h>

namespace rad
{

struct ShaderManager : Singleton<ShaderManager>
{
	ShaderManager();
	
	Shader* CompileShader(std::wstring_view name, std::wstring_view shaderPath, ShaderType type, std::wstring_view entryPoint = L"main", std::span<const std::wstring_view> includeFolders = {});

	std::unordered_map<std::wstring, std::unique_ptr<Shader>> LoadedShaders;

private:
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcCompiler3> Compiler;
	ComPtr<IDxcIncludeHandler> IncludeHandler;
};

}