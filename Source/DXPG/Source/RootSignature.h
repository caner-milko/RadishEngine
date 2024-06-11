#pragma once

#include "DXHelpers.h"
#include "Shader.h"

namespace dxpg
{
struct RootSignature
{
	std::string Name;

	ComPtr<ID3D12RootSignature> DXSignature;
	std::unordered_map<std::string, uint32_t> NameToParameterIndices;
};

struct RootSignatureBuilder
{
	std::vector<CD3DX12_ROOT_PARAMETER1> Parameters;
	std::vector<std::string> ParameterNames;
	std::vector<std::vector<CD3DX12_DESCRIPTOR_RANGE1>> DescriptorRanges;
	std::vector<CD3DX12_STATIC_SAMPLER_DESC> StaticSamplers;

	RootSignatureBuilder& AddDescriptorTable(std::string_view name, std::span<const CD3DX12_DESCRIPTOR_RANGE1> ranges, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureBuilder& AddStaticSampler(CD3DX12_STATIC_SAMPLER_DESC const& sampler);
	RootSignatureBuilder& AddRootParameter(std::string_view name, CD3DX12_ROOT_PARAMETER1 const& parameter);

	struct RootParamDesc
	{
		UINT ShaderRegister;
		UINT RegisterSpace = 0;
		D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL;
		D3D12_ROOT_DESCRIPTOR_FLAGS DescFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	};

	RootSignatureBuilder& AddConstants(std::string_view name, UINT num32BitValues, RootParamDesc desc);
	RootSignatureBuilder& AddConstantBufferView(std::string_view name, RootParamDesc desc);
	RootSignatureBuilder& AddShaderResourceView(std::string_view name, RootParamDesc desc);
	RootSignatureBuilder& AddUnorderedAccessView(std::string_view name, RootParamDesc desc);


	RootSignature Build(std::string_view name, ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags);

};


}