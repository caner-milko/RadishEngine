#pragma once

#include "DXHelpers.h"
#include "Shader.h"
#include "RadishCommon.h"
#include "RootSignature.h"
#include "RendererCommon.h"

#include <dxcapi.h>
#include <d3d12shader.h>

namespace rad
{

struct ShaderManager
{
	ShaderManager(Renderer& renderer);
	bool Init();

	std::pair<Shader*, Shader*> CompileBindlessGraphicsShader(std::wstring_view name, std::wstring_view shaderPath,
															  std::span<const std::wstring_view> includeFolders = {});
	Shader* CompileBindlessVertexShader(std::wstring_view name, std::wstring_view shaderPath,
										std::wstring_view entryPoint = L"VSMain",
										std::span<const std::wstring_view> includeFolders = {});
	Shader* CompileBindlessComputeShader(std::wstring_view name, std::wstring_view shaderPath,
										 std::wstring_view entryPoint = L"CSMain",
										 std::span<const std::wstring_view> includeFolders = {});
	Shader* CompileShader(std::wstring_view name, std::wstring_view shaderPath, ShaderType type,
						  std::wstring_view entryPoint = L"main",
						  std::span<const std::wstring_view> includeFolders = {});

	std::unordered_map<std::wstring, std::unique_ptr<Shader>> LoadedShaders;

	RootSignature BindlessRootSignature;

  private:
	Renderer& Renderer;
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcCompiler3> Compiler;
	ComPtr<IDxcIncludeHandler> IncludeHandler;
};

} // namespace rad