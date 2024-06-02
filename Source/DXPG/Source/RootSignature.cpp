#include "RootSignature.h"
namespace dxpg::dx12
{
RootSignature RootSignature::CreateForGraphics(Shader const& vertexShader, Shader const& pixelShader, std::span<DescriptorTable> descriptorTables, std::span<StaticSampler> samplerDescriptions)
{
	RootSignature rs{};

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};

	std::unordered_map<std::string, std::vector<CD3DX12_DESCRIPTOR_RANGE1>> descriptorRanges;

	for (auto const& [name, binding] : vertexShader.ResourceBindings)
	{
        std::optional<std::string> descriptorTableName;

		for (auto& table : descriptorTables)
            if (table.Name == name)
            {
				descriptorTableName = name;
				break;
			}

		switch (binding.Type)
		{
        case D3D_SIT_CBUFFER:
        {
			if (!descriptorTableName)
            {
                CD3DX12_ROOT_PARAMETER1 rootParameter;
                rootParameter.InitAsConstants
                rootParameter.InitAsConstantBufferView(binding.BindPoint);
			    rs.NameToParameterIndices[name] = rs.Parameters.size();
			    rs.Parameters.push_back(rootParameter);
            }
            else
            {
				CD3DX12_DESCRIPTOR_RANGE1 descriptorRange;
				descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, binding.BindPoint, binding.Space);
				descriptorRanges[descriptorTableName.value()].push_back(descriptorRange);
            }
            break;
        }
        case D3D_SIT_TBUFFER:
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
        {
			if (!descriptorTableName)
			{
                CD3DX12_ROOT_PARAMETER1 rootParameter;
                CD3DX12_ROOT_DESCRIPTOR1 rootDescriptor;
			    rootDescriptor.Init(binding.BindPoint, binding.Space, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
                
			}
            else
            {

            }
            break;
        }
        case D3D_SIT_SAMPLER:
            descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, bindDesc.BindPoint);
            descriptorRanges.push_back(descriptorRange);
            rootParameter.InitAsDescriptorTable(1, &descriptorRanges.back());
            rootParameters.push_back(rootParameter);
            break;
        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWBYTEADDRESS:
        case D3D_SIT_UAV_APPEND_STRUCTURED:
        case D3D_SIT_UAV_CONSUME_STRUCTURED:
        case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, bindDesc.BindPoint);
            descriptorRanges.push_back(descriptorRange);
            rootParameter.InitAsDescriptorTable(1, &descriptorRanges.back());
            rootParameters.push_back(rootParameter);
            break;
        default:
            std::cerr << "Unsupported resource type." << std::endl;
            break;
		}
	}


	return rs;
}
}
