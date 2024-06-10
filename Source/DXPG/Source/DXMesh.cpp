#include "DXMesh.h"

namespace dxpg
{
std::unique_ptr<VertexData> VertexData::Create(ID3D12Device* device, size_t positionsCount, size_t normalsCount, size_t texCoordsCount)
{
    auto vertexData = std::make_unique<VertexData>();
    size_t posBytes = positionsCount * 3 * sizeof(float);
    D3D12_RESOURCE_DESC posDesc = CD3DX12_RESOURCE_DESC::Buffer(posBytes);

    size_t normalBytes = normalsCount * 3 * sizeof(float);
    D3D12_RESOURCE_DESC normalDesc = CD3DX12_RESOURCE_DESC::Buffer(normalBytes);

    size_t texCoordBytes = texCoordsCount * 2 * sizeof(float);
    D3D12_RESOURCE_DESC texCoordDesc = CD3DX12_RESOURCE_DESC::Buffer(texCoordBytes);

    D3D12_RESOURCE_DESC descs[] = { posDesc, normalDesc, texCoordDesc };

    auto posAllocInfo = device->GetResourceAllocationInfo(0, 1, &posDesc);
    auto normalAllocInfo = device->GetResourceAllocationInfo(0, 1, &normalDesc);
    auto texCoordAllocInfo = device->GetResourceAllocationInfo(0, 1, &texCoordDesc);


    auto allocInfo = device->GetResourceAllocationInfo(0, _countof(descs), descs);


    auto heapDesc = CD3DX12_HEAP_DESC(allocInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES);
    device->CreateHeap(&heapDesc, IID_PPV_ARGS(&vertexData->Heap));

    device->CreatePlacedResource(vertexData->Heap.Get(), 0, &posDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&vertexData->PositionsBuffer));

    device->CreatePlacedResource(vertexData->Heap.Get(), posAllocInfo.SizeInBytes, &normalDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&vertexData->NormalsBuffer));

    device->CreatePlacedResource(vertexData->Heap.Get(), posAllocInfo.SizeInBytes + normalAllocInfo.SizeInBytes, &texCoordDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&vertexData->TexCoordsBuffer));

    ResourceViewToDesc<ViewTypes::ShaderResourceView> resources[3] = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC posViewDesc{};
    posViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    posViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    posViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    posViewDesc.Buffer.StructureByteStride = 3 * sizeof(float);
    posViewDesc.Buffer.FirstElement = 0;
    posViewDesc.Buffer.NumElements = positionsCount;
    resources[0] = { .Desc = &posViewDesc, .Resource = vertexData->PositionsBuffer.Get() };
    auto normalsViewDesc = posViewDesc;
    normalsViewDesc.Buffer.NumElements = normalsCount;
    resources[1] = { &normalsViewDesc, vertexData->NormalsBuffer.Get() };
    auto texCoordsViewDesc = normalsViewDesc;
    texCoordsViewDesc.Buffer.StructureByteStride = 2 * sizeof(float);
    texCoordsViewDesc.Buffer.NumElements = texCoordsCount;
    resources[2] = { &texCoordsViewDesc, vertexData->TexCoordsBuffer.Get() };

    vertexData->VertexSRV = ShaderResourceView::Create(std::span{ resources });
    return vertexData;
}

std::unique_ptr<D3D12Mesh> D3D12Mesh::Create(ID3D12Device* device, dxpg::VertexData* vertexData, size_t indicesCount, size_t indexSize)
{
    auto mesh = std::unique_ptr<D3D12Mesh>(new D3D12Mesh());
    mesh->VertexData = vertexData;
    mesh->IndicesCount = indicesCount;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(indicesCount * indexSize);
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mesh->Indices));
    mesh->IndicesView.BufferLocation = mesh->Indices->GetGPUVirtualAddress();
    mesh->IndicesView.SizeInBytes = indicesCount * indexSize;
    mesh->IndicesView.StrideInBytes = indexSize;
    return mesh;
}
}