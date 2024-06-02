#include "ShaderCompiler.h"

#include <unordered_map>
#include "DXPGCommon.h"

namespace dxpg::dx12
{
std::unique_ptr<ShaderCompiler> ShaderCompiler::Create()
{
	auto compiler = new ShaderCompiler();
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&compiler->Utils)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler->Compiler)));
	ThrowIfFailed(compiler->Utils->CreateDefaultIncludeHandler(&compiler->IncludeHandler));

	return std::unique_ptr<ShaderCompiler>(compiler);
}
std::unique_ptr<Shader> ShaderCompiler::CompileShader(std::wstring_view name, std::wstring_view shaderPath, ShaderType type, std::wstring_view entryPoint, std::span<std::wstring_view> includeFolders)
{
	LPCWSTR shaderType = nullptr;
	switch(type)
	{
	case ShaderType::Vertex:
		shaderType = L"vs_6_0";
		break;
	case ShaderType::Pixel:
		shaderType = L"ps_6_0";
		break;
	case ShaderType::Compute:
		shaderType = L"cs_6_0";
		break;
	}

	std::vector<LPCWSTR> compilationArgs =
	{
		name.data(),                  // Optional shader source file name for error reporting and for PIX shader source view.  
		L"-E", entryPoint.data(),              // Entry point.
		L"-T", shaderType,            // Target.
		DXC_ARG_WARNINGS_ARE_ERRORS,
		DXC_ARG_ALL_RESOURCES_BOUND,
	};
	
	for (auto& includeFolder : includeFolders)
	{
		compilationArgs.push_back(L"-I");
		compilationArgs.push_back(includeFolder.data());
	}

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

	ComPtr<IDxcResult> results;
	Compiler->Compile(&Source, compilationArgs.data(), compilationArgs.size(), IncludeHandler.Get(), IID_PPV_ARGS(&results));

	ComPtr<IDxcBlobUtf8> pErrors = nullptr;
	results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

	HRESULT hrStatus;
	results->GetStatus(&hrStatus);
	if (FAILED(hrStatus))
	{
		wprintf(L"Compilation Failed\n");
		return nullptr;
	}

	// Get shader reflection data.
	ComPtr<IDxcBlob> reflectionBlob{};
	ThrowIfFailed(results->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr));

	const DxcBuffer reflectionBuffer
	{
		.Ptr = reflectionBlob->GetBufferPointer(),
		.Size = reflectionBlob->GetBufferSize(),
		.Encoding = 0,
	};

	ComPtr<ID3D12ShaderReflection> shaderReflection{};
	Utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
	D3D12_SHADER_DESC shaderDesc{};
	shaderReflection->GetDesc(&shaderDesc);

	ComPtr<ID3DBlob> compiledBlob;

	results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledBlob), 0);

	if (type == ShaderType::Vertex)
	{

		for (UINT i = 0; i < shaderDesc.InputParameters; i++)
		{
			D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
			shaderReflection->GetInputParameterDesc(i, &paramDesc);

			auto& desc = inputElements[paramDesc.SemanticName];

			desc.SemanticName = paramDesc.SemanticName;
			desc.SemanticIndex = paramDesc.SemanticIndex;


			desc.Format = paramDesc.Mask == 1 ? DXGI_FORMAT_R32_FLOAT : (paramDesc.Mask <= 3 ? DXGI_FORMAT_R32G32_FLOAT : (paramDesc.Mask <= 7 ? DXGI_FORMAT_R32G32B32_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT));
			switch (paramDesc.ComponentType)
			{
			case D3D_REGISTER_COMPONENT_FLOAT32:
				break;
			case D3D_REGISTER_COMPONENT_UINT32:
				desc.Format = DXGI_FORMAT(desc.Format + 1);
				break;
			case D3D_REGISTER_COMPONENT_SINT32:
				desc.Format = DXGI_FORMAT(desc.Format + 2);
				break;
			default:
				__debugbreak();
			break;
			}
			
			desc.InputSlot = 0;
			desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
			desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			desc.InstanceDataStepRate = 0;
		}
	}

	std::unordered_map<std::string, uint32_t> rootParameterIndexMap;

	std::vector<D3D12_ROOT_PARAMETER1> rootParameters;

	for (UINT i = 0; i < shaderDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
		ThrowIfFailed(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));

		switch (shaderInputBindDesc.Type)
		{
		case D3D_SIT_CBUFFER:
		{
			rootParameterIndexMap[shaderInputBindDesc.Name] = static_cast<uint32_t>(rootParameters.size());
			ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

			const D3D12_ROOT_PARAMETER1 rootParameter
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
				.Descriptor{
					.ShaderRegister = shaderInputBindDesc.BindPoint,
					.RegisterSpace = shaderInputBindDesc.Space,
					.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
				},
			};

			rootParameters.push_back(rootParameter);
			break;
		}
		case D3D_SIT_TEXTURE:
		{
			rootParameterIndexMap[shaderInputBindDesc.Name] = static_cast<uint32_t>(rootParameters.size());
			CD3DX12_DESCRIPTOR_RANGE1 range{};
			range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, shaderInputBindDesc.BindPoint, shaderInputBindDesc.Space, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			
			const D3D12_ROOT_PARAMETER1 rootParameter
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
				.Descriptor{
					.ShaderRegister = shaderInputBindDesc.BindPoint,
					.RegisterSpace = shaderInputBindDesc.Space,
					.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
				},
			};

			rootParameters.push_back(rootParameter);
		}
		case D3D_SIT_STRUCTURED:
		{
			rootParameterIndexMap[shaderInputBindDesc.Name] = static_cast<uint32_t>(rootParameters.size());
			const D3D12_ROOT_PARAMETER1 rootParameter
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
				.Descriptor{
					.ShaderRegister = shaderInputBindDesc.BindPoint,
					.RegisterSpace = shaderInputBindDesc.Space,
					.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
				},
			};

			rootParameters.push_back(rootParameter);
		}

		default:
			break;
		}
	}


	auto shader = std::make_unique<Shader>();

	shader->EntryPoint = entryPoint;
	shader->Path = shaderPath;
	shader->Name = name;
	shader->Blob = compiledBlob;
    return shader;
}

}