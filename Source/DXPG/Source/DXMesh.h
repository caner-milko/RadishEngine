#pragma once

#include "DXHelpers.h"

namespace dxpg
{
struct VertexData
{
    static std::unique_ptr<VertexData> Create(ID3D12Device* device, size_t positionsCount, size_t normalsCount, size_t texCoordsCount);
    ComPtr<ID3D12Heap> Heap;
    ComPtr<ID3D12Resource> PositionsBuffer;
    ComPtr<ID3D12Resource> NormalsBuffer;
    ComPtr<ID3D12Resource> TexCoordsBuffer;

    std::unique_ptr<ShaderResourceView> VertexSRV;
};

struct D3D12Mesh
{
    static std::unique_ptr<D3D12Mesh> Create(ID3D12Device* device, VertexData* vertexData, size_t indicesCount, size_t indexSize);
    VertexData* VertexData = nullptr;
    ComPtr<ID3D12Resource> Indices;
    D3D12_VERTEX_BUFFER_VIEW IndicesView{};
    size_t IndicesCount = 0;

    Vector4 Position = { 0, 0, 0, 1 };
    Vector4 Rotation = { 0, 0, 0, 0 };
    Vector4 Scale = { 1, 1, 1, 0 };

    Matrix4x4 GetWorldMatrix()
    {
        Matrix4x4 translation = DirectX::XMMatrixTranslationFromVector(Position);
        Matrix4x4 rotation = DirectX::XMMatrixRotationRollPitchYawFromVector(Rotation);
        Matrix4x4 scale = DirectX::XMMatrixScalingFromVector(Scale);
        return scale * rotation * translation;
    }
private:
    D3D12Mesh() = default;
};

struct D3D12Material
{
    std::unique_ptr<ShaderResourceView> DiffuseSRV = nullptr;
    std::unique_ptr<ShaderResourceView> AlphaSRV = nullptr;
};
}