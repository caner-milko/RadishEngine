#include "ShaderManager.h"

#include "Renderer.h"
#include "RadishCommon.h"
#include <unordered_map>
#include <filesystem>

namespace rad
{
ShaderManager::ShaderManager(rad::Renderer& renderer) : Renderer(renderer)
{
}

bool ShaderManager::Init()
{
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
	ThrowIfFailed(Utils->CreateDefaultIncludeHandler(&IncludeHandler));
	auto vertexShader = CompileBindlessVertexShader(L"Fullscreen", RAD_SHADERS_DIR L"FullscreenVS.hlsli");
	ThrowIfFailed(Renderer.GetDevice().CreateRootSignature(0u, vertexShader->RootSignatureBlob->GetBufferPointer(),
		vertexShader->RootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&BindlessRootSignature.DXSignature)));
	BindlessRootSignature.DXSignature->SetName(L"BindlessRootSignature");
	return true;
}

std::pair<Shader*, Shader*> ShaderManager::CompileBindlessGraphicsShader(std::wstring_view name, std::wstring_view shaderPath, std::span<const std::wstring_view> includeFolders)
{
	std::vector<std::wstring_view> includeFoldersCopy{ {RAD_SHADERS_DIR L""} };
	includeFoldersCopy.insert(includeFoldersCopy.end(), includeFolders.begin(), includeFolders.end());
	auto vs = CompileShader(std::wstring(name) + L".vs", shaderPath, ShaderType::Vertex, L"VSMain", includeFoldersCopy);
	auto ps = CompileShader(std::wstring(name) + L".ps", shaderPath, ShaderType::Pixel, L"PSMain", includeFoldersCopy);
	return { vs, ps };
}

Shader* ShaderManager::CompileBindlessVertexShader(std::wstring_view name, std::wstring_view shaderPath, std::wstring_view entryPoint, std::span<const std::wstring_view> includeFolders)
{
	std::vector<std::wstring_view> includeFoldersCopy{ {RAD_SHADERS_DIR L""}};
	includeFoldersCopy.insert(includeFoldersCopy.end(), includeFolders.begin(), includeFolders.end());
	return CompileShader(std::wstring(name) + L".vs", shaderPath, ShaderType::Vertex, entryPoint, includeFoldersCopy);
}

Shader* ShaderManager::CompileBindlessComputeShader(std::wstring_view name, std::wstring_view shaderPath, std::wstring_view entryPoint, std::span<const std::wstring_view> includeFolders)
{
	std::vector<std::wstring_view> includeFoldersCopy{ {RAD_SHADERS_DIR L""} };
	includeFoldersCopy.insert(includeFoldersCopy.end(), includeFolders.begin(), includeFolders.end());
	return CompileShader(std::wstring(name) + L".cs", shaderPath, ShaderType::Compute, entryPoint, includeFoldersCopy);
}

Shader* ShaderManager::CompileShader(std::wstring_view name, std::wstring_view shaderPath, ShaderType type, std::wstring_view entryPoint, std::span<const std::wstring_view> includeFolders)
{
	LPCWSTR shaderType = nullptr;
	switch (type)
	{
	case ShaderType::Vertex:
		shaderType = L"vs_6_6";
		break;
	case ShaderType::Pixel:
		shaderType = L"ps_6_6";
		break;
	case ShaderType::Compute:
		shaderType = L"cs_6_6";
		break;
	}

	std::vector<LPCWSTR> compilationArgs =
	{
		name.data(),                  // Optional shader source file name for error reporting and for PIX shader source view.  
		L"-E", entryPoint.data(),              // Entry point.
		L"-T", shaderType,            // Target.
		//DXC_ARG_WARNINGS_ARE_ERRORS,
		DXC_ARG_ALL_RESOURCES_BOUND,
	};

	for (auto& includeFolder : includeFolders)
	{
		compilationArgs.push_back(L"-I");
		compilationArgs.push_back(includeFolder.data());
	}

	std::filesystem::path parentPath = std::filesystem::path(shaderPath).parent_path();
	compilationArgs.push_back(L"-I");
	compilationArgs.push_back(parentPath.c_str());

	if constexpr (_DEBUG)
		compilationArgs.push_back(DXC_ARG_DEBUG);
	else
		compilationArgs.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);

	ComPtr<IDxcBlobEncoding> sourceEncoded;
	Utils->LoadFile(shaderPath.data(), nullptr, &sourceEncoded);
	if (!sourceEncoded)
	{
		wprintf(L"Failed to load shader file %s\n", shaderPath.data());
		return nullptr;
	}

	DxcBuffer Source;
	Source.Ptr = sourceEncoded->GetBufferPointer();
	Source.Size = sourceEncoded->GetBufferSize();
	Source.Encoding = DXC_CP_ACP;

	wprintf(L"Compiling %s\n", shaderPath.data());
	ComPtr<IDxcResult> results;
	Compiler->Compile(&Source, compilationArgs.data(), compilationArgs.size(), IncludeHandler.Get(), IID_PPV_ARGS(&results));

	ComPtr<IDxcBlobUtf8> pErrors = nullptr;
	results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	if (pErrors != nullptr && pErrors->GetStringLength() != 0)
	{
		wprintf(L"%S", pErrors->GetStringPointer());
	}

	ComPtr<ID3DBlob> rootSignatureBlob{ nullptr };
	results->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(&rootSignatureBlob), nullptr);

	HRESULT hrStatus;
	results->GetStatus(&hrStatus);
	if (FAILED(hrStatus))
	{
		wprintf(L"Compilation Failed\n");
		return nullptr;
	}

	ComPtr<ID3DBlob> compiledBlob;

	results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledBlob), 0);

	auto& shader = LoadedShaders[std::wstring(name)] = std::make_unique<Shader>();

	shader->EntryPoint = entryPoint;
	shader->Path = shaderPath;
	shader->Name = name;
	shader->Blob = compiledBlob;
	shader->RootSignatureBlob = rootSignatureBlob;
	shader->Type = type;
	return shader.get();
}

}