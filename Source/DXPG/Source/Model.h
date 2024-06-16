#pragma once

#include "DXResource.h"
#include "RendererCommon.h"

namespace dxpg
{


struct HLSL_ShaderMaterialInfo
{
    Vector4 Diffuse;
    int UseDiffuseTexture;
    int UseAlphaTexture;
};

struct Material
{
    std::string Name;
    std::optional<std::string> DiffuseTextureName;

    bool Dirty = false;

    Vector3 DiffuseColor = { 1, 1, 1 };

    DXTypedBuffer<HLSL_ShaderMaterialInfo> MaterialInfoBuffer;
    DescriptorAllocation MaterialInfo;
	std::optional<DescriptorAllocation> DiffuseTextureSRV = std::nullopt;
};

struct Model
{
    DXTypedBuffer<Vector3> PositionsBuffer;
    DXTypedBuffer<Vector3> NormalsBuffer;
    DXTypedBuffer<Vector2> TexCoordsBuffer;
    DXTypedBuffer<Vector3> TangentsBuffer;

    DescriptorAllocation VertexSRV;
};

struct IndexedModel
{
	std::string Name;
    Model* Model;
    DXTypedBuffer<HLSL_VertexData> Indices;
    D3D12_VERTEX_BUFFER_VIEW IndicesView{};
};
}