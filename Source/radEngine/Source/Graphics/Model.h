#pragma once

#include "DXResource.h"
#include "RendererCommon.h"

#include "ConstantBuffers.hlsli"

namespace rad
{

struct Material
{
    std::string Name;
    std::optional<std::string> DiffuseTextureName;
	std::optional<std::string> NormalMapTextureName;

    Vector3 DiffuseColor = { 1, 1, 1 };

    DXTypedSingularBuffer<rad::hlsl::MaterialBuffer> MaterialInfoBuffer;
    DescriptorAllocation MaterialInfo;
	std::optional<DescriptorAllocation> DiffuseTextureSRV = std::nullopt;
	std::optional<DescriptorAllocation> NormalMapTextureSRV = std::nullopt;
};

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
	DirectX::XMFLOAT3 Tangent;

	bool operator==(const Vertex& other) const
	{
		return Position.x == other.Position.x && Position.y == other.Position.y && Position.z == other.Position.z &&
			Normal.x == other.Normal.x && Normal.y == other.Normal.y && Normal.z == other.Normal.z &&
			TexCoord.x == other.TexCoord.x && TexCoord.y == other.TexCoord.y;
	}
};

} // namespace rad