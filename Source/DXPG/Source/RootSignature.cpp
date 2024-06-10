#include "RootSignature.h"

#include "DXPGCommon.h"
namespace dxpg
{

RootSignatureBuilder& RootSignatureBuilder::AddDescriptorTable(std::string_view name, std::span<const CD3DX12_DESCRIPTOR_RANGE1> ranges, D3D12_SHADER_VISIBILITY shaderVisibility)
{
    CD3DX12_ROOT_PARAMETER1 parameter;
    parameter.InitAsDescriptorTable(static_cast<UINT>(ranges.size()), DescriptorRanges.emplace_back(ranges.begin(), ranges.end()).data(), shaderVisibility);
    Parameters.push_back(parameter);
    ParameterNames.push_back(name.data());
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddStaticSampler(CD3DX12_STATIC_SAMPLER_DESC const& sampler)
{
    StaticSamplers.push_back(sampler);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddRootParameter(std::string_view name, CD3DX12_ROOT_PARAMETER1 const& parameter)
{
    Parameters.push_back(parameter);
    ParameterNames.push_back(name.data());
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddConstants(std::string_view name, UINT num32BitValues, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter;
	parameter.InitAsConstants(num32BitValues, shaderRegister, registerSpace, shaderVisibility);
	return AddRootParameter(name, parameter);
}

RootSignatureBuilder& RootSignatureBuilder::AddConstantBufferView(std::string_view name, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter;
	parameter.InitAsConstantBufferView(shaderRegister, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, shaderVisibility);
	return AddRootParameter(name, parameter);
}

RootSignatureBuilder& RootSignatureBuilder::AddShaderResourceView(std::string_view name, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter;
	parameter.InitAsShaderResourceView(shaderRegister, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, shaderVisibility);
	return AddRootParameter(name, parameter);
}

RootSignatureBuilder& RootSignatureBuilder::AddUnorderedAccessView(std::string_view name, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter;
	parameter.InitAsUnorderedAccessView(shaderRegister, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, shaderVisibility);
	return AddRootParameter(name, parameter);
}

RootSignature RootSignatureBuilder::Build(std::string_view name, ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	RootSignature rs{};
	rs.Name = name;
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Init_1_1(static_cast<UINT>(Parameters.size()), Parameters.data(), static_cast<UINT>(StaticSamplers.size()), StaticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signatureBlob, &errorBlob));
	ThrowIfFailed(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rs.DXSignature)));
	rs.DXSignature->SetName(s2ws(rs.Name).c_str());

	for (size_t i = 0; i < ParameterNames.size(); i++)
		rs.NameToParameterIndices[ParameterNames[i]] = static_cast<uint32_t>(i);

	return rs;
}

}