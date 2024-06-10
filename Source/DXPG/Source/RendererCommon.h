#pragma once

#include "DXHelpers.h"

namespace dxpg
{

struct ViewData
{
    Matrix4x4 View;
    Matrix4x4 Projection;
	Matrix4x4 ViewProjection;
	Vector4 Position;
	Vector4 Direction;
    float FoV;
};

struct FrameContext
{
    bool Ready = false;
    ComPtr<ID3D12CommandAllocator> CommandAllocator;
    UINT64                  FenceValue;
    std::unordered_map<D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorHeapPage*> GPUHeapPages = {};
    std::unordered_map<DescriptorAllocation*, std::unique_ptr<DescriptorAllocation>> CPUViewsToGPUViews = {};

	std::vector<ComPtr<ID3D12Resource>> IntermediateResources;

    DescriptorAllocation* GetGPUAllocation(DescriptorAllocation* cpuAllocation)
    {
        assert(cpuAllocation);
        auto it = CPUViewsToGPUViews.find(cpuAllocation);
        if (it == CPUViewsToGPUViews.end())
            return (CPUViewsToGPUViews[cpuAllocation] = GPUHeapPages[cpuAllocation->Heap->Desc.Type]->CopyFrom(cpuAllocation)).get();
        return it->second.get();
    }
};

struct MeshGroupView
{
    struct MeshView
    {
        D3D12_VERTEX_BUFFER_VIEW IndexBufferView;
        size_t IndexCount;
        Matrix4x4 ModelMatrix;

        bool UseDiffuseTexture = false;
        Vector3 DiffuseColor{};
        D3D12_GPU_DESCRIPTOR_HANDLE DiffuseSRV{};

        bool UseAlphaTexture = false;
        D3D12_GPU_DESCRIPTOR_HANDLE AlphaSRV{};
    };
    D3D12_GPU_DESCRIPTOR_HANDLE VertexSRV;
	std::vector<MeshView> Meshes;
};

struct SceneDataView
{
    std::vector<MeshGroupView> MeshGroups;
};

}