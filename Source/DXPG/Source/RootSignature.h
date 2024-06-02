#pragma once

#include "DXHelpers.h"
#include "Shader.h"

namespace dxpg::dx12
{

struct DescriptorTable
{
	std::string Name;
	std::vector<std::string> Descriptors;
};

struct StaticSampler
{
	std::string Name;
	D3D12_STATIC_SAMPLER_DESC Description;
};

struct RootSignature
{
	static RootSignature CreateForGraphics(Shader const& vertexShader, Shader const& pixelShader, std::span<DescriptorTable> descriptorTables = {}, std::span<StaticSampler> samplerDescriptions = {});

	static RootSignature CreateForCompute(Shader const& computeShader, std::span<DescriptorTable> descriptorTables = {}, std::span<StaticSampler> samplerDescriptions = {});
	ComPtr<ID3D12RootSignature> Signature;
	
	std::unordered_map<std::string, uint32_t> NameToParameterIndices;
	std::vector<D3D12_ROOT_PARAMETER1> Parameters;
	std::vector<D3D12_STATIC_SAMPLER_DESC> StaticSamplers;

private:
	RootSignature() = default;

};
}