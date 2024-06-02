#pragma once
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <span>
#include <unordered_map>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include "DirectXMath.h"
using namespace DirectX;
using Matrix4x4 = DirectX::XMMATRIX;
using Vector4 = DirectX::XMVECTOR;
using Vector3 = DirectX::XMFLOAT3;
using Vector2 = DirectX::XMFLOAT2;
#include <directx/d3dx12.h>
#include <d3dcompiler.h>

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

namespace dxpg::dx12
{
struct DescriptorHeap
{
    static std::unique_ptr<DescriptorHeap> Create(D3D12_DESCRIPTOR_HEAP_DESC desc, ID3D12Device* device);

    ID3D12Device* Device;
    ComPtr<ID3D12DescriptorHeap> Heap;
    D3D12_DESCRIPTOR_HEAP_DESC Desc;
    size_t Increment;

    size_t Top = 0;

    size_t Allocate(uint32_t count = 1)
    {
        size_t old = Top;
        Top += count;
        assert(GetSize() >= Top);
        return old;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(size_t index)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = Heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * Increment;
        return handle;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(size_t index)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = Heap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += index * Increment;
        return handle;
    }
    size_t GetSize() const { return Desc.NumDescriptors; }
};

struct DescriptorAllocation
{
    static std::unique_ptr<DescriptorAllocation> Create(DescriptorHeap* heap, uint32_t size);
    static std::unique_ptr<DescriptorAllocation> CreatePreAllocated(DescriptorHeap* heap, size_t index, size_t size);
	
    DescriptorHeap* Heap;
	size_t Index;
	size_t Size;

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(size_t offset = 0)
	{
		return Heap->GetCPUHandle(Index + offset);
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(size_t offset = 0)
	{
		return Heap->GetGPUHandle(Index + offset);
	}
private:
    DescriptorAllocation() = default;
};

struct DescriptorHeapPage
{
    DescriptorHeap* Heap{};
    size_t Offset{};
    size_t Size{};
    size_t Top = 0;

    static std::unique_ptr<DescriptorHeapPage> Create(DescriptorHeap* heap, size_t size)
    {
		auto page = std::unique_ptr<DescriptorHeapPage>(new DescriptorHeapPage);
		page->Heap = heap;
		page->Size = size;
		page->Offset = heap->Allocate(size);
		return page;
	}

    std::unique_ptr<DescriptorAllocation> Allocate(uint32_t count)
    {
		size_t old = Top;
		Top += count;
		assert(Size >= Top);
		return DescriptorAllocation::CreatePreAllocated(Heap, old + Offset, count);
    }

    std::unique_ptr<DescriptorAllocation> CopyFrom(DescriptorAllocation* alloc)
	{
    	auto newAlloc = Allocate(alloc->Size);
        Heap->Device->CopyDescriptorsSimple(alloc->Size, newAlloc->GetCPUHandle(), alloc->GetCPUHandle(), Heap->Desc.Type);
        return newAlloc;
    }

    void Reset()
    {
        Top = 0;
    }
private:
    DescriptorHeapPage() = default;
};

struct DescriptorHeapPageCollection
{
    
    std::shared_ptr<DescriptorHeap> Heap;
    std::shared_ptr<DescriptorHeapPage> StaticPage;
    std::vector<std::shared_ptr<DescriptorHeapPage>> Pages;

    std::vector<DescriptorHeapPage*> FreePages;
    std::vector<DescriptorHeapPage*> UsedPages;

    static std::unique_ptr<DescriptorHeapPageCollection> Create(D3D12_DESCRIPTOR_HEAP_DESC desc, ID3D12Device* device, size_t pageCount, size_t staticPageSize)
    {
		auto collection = std::make_unique<DescriptorHeapPageCollection>();
		collection->Heap = DescriptorHeap::Create(desc, device);
        size_t remSize = desc.NumDescriptors - staticPageSize;
        if (pageCount == 0)
            remSize = 0;
        else
            remSize = remSize - remSize % pageCount;
        collection->StaticPage = DescriptorHeapPage::Create(collection->Heap.get(), desc.NumDescriptors - remSize);
        for (size_t i = 0; i < pageCount; i++)
        {
            collection->Pages.push_back(DescriptorHeapPage::Create(collection->Heap.get(), remSize / pageCount));
            collection->FreePages.push_back(collection->Pages.back().get());
        }
		return collection;
	}

    DescriptorHeapPage* AllocatePage()
    {
		if (FreePages.empty())
			return nullptr;
		auto page = FreePages.back();
		FreePages.pop_back();
		UsedPages.push_back(page);
        return page;
	}

    void FreePage(DescriptorHeapPage* page)
    {
		auto it = std::find(UsedPages.begin(), UsedPages.end(), page);
        if (it == UsedPages.end())
        {
            assert(false);
            return;
        }
		UsedPages.erase(it);
		FreePages.push_back(page);
        page->Reset();
	}

    std::unique_ptr<DescriptorAllocation> AllocateFromStatic(uint32_t count)
    {
        return StaticPage->Allocate(count);
	}

};

template<bool CPU>
struct DescriptorHeapAllocator
{
    std::unordered_map<D3D12_DESCRIPTOR_HEAP_TYPE, std::unique_ptr<DescriptorHeapPageCollection>> Heaps;

    static std::unique_ptr<DescriptorHeapAllocator> Create(ID3D12Device* dev, size_t numDescriptors, uint32_t pageCount = 0, size_t staticPageSize = 0);

    std::unique_ptr<DescriptorAllocation> AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size)
    {
		return Heaps[type]->AllocateFromStatic(size);
	}

    std::vector<ID3D12DescriptorHeap*> GetHeaps()
    {
		std::vector<ID3D12DescriptorHeap*> heaps;
		for (auto& [type, heap] : Heaps)
			heaps.push_back(heap->Heap->Heap.Get());
		return heaps;
	}
};
using CPUDescriptorHeapAllocator = DescriptorHeapAllocator<true>;
using GPUDescriptorHeapAllocator = DescriptorHeapAllocator<false>;

extern std::unique_ptr<CPUDescriptorHeapAllocator> g_CPUDescriptorAllocator;
extern std::unique_ptr<GPUDescriptorHeapAllocator> g_GPUDescriptorAllocator;

enum class ViewTypes
{
    ShaderResourceView,
    UnorderedAccessView,
    ConstantBufferView,
    Sampler,
	RenderTargetView,
	DepthStencilView
};

template<ViewTypes type>
struct ResourceViewToDesc;

template<> struct ResourceViewToDesc<ViewTypes::ShaderResourceView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	D3D12_SHADER_RESOURCE_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};
template<> struct ResourceViewToDesc<ViewTypes::UnorderedAccessView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	D3D12_UNORDERED_ACCESS_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};
template<> struct ResourceViewToDesc<ViewTypes::ConstantBufferView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	D3D12_CONSTANT_BUFFER_VIEW_DESC* Desc;
};
template<> struct ResourceViewToDesc<ViewTypes::Sampler> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	D3D12_SAMPLER_DESC* Desc;
};
template<> struct ResourceViewToDesc<ViewTypes::RenderTargetView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	D3D12_RENDER_TARGET_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};
template<> struct ResourceViewToDesc<ViewTypes::DepthStencilView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	D3D12_DEPTH_STENCIL_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};

template<ViewTypes type>
struct ResourceView : DescriptorAllocation
{
    static std::unique_ptr<ResourceView<type>> Create(ID3D12Device* device, typename ResourceViewToDesc<type> descs)
    {
		return Create(device, std::span{ &descs, 1 });
	}
    static std::unique_ptr<ResourceView<type>> Create(ID3D12Device* device, std::span<typename ResourceViewToDesc<type>> descs);
};

using ShaderResourceView = ResourceView<ViewTypes::ShaderResourceView>;
using UnorderedAccessView = ResourceView<ViewTypes::UnorderedAccessView>;
using ConstantBufferView = ResourceView<ViewTypes::ConstantBufferView>;
using Sampler = ResourceView<ViewTypes::Sampler>;
using RenderTargetView = ResourceView<ViewTypes::RenderTargetView>;
using DepthStencilView = ResourceView<ViewTypes::DepthStencilView>;


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

struct D3D12Texture
{
	static std::unique_ptr<D3D12Texture> Create(ID3D12Device* device, DXGI_FORMAT format, size_t width, size_t height);



	ComPtr<ID3D12Resource> Resource;
	std::unique_ptr<ShaderResourceView> SRV;
private:
	D3D12Texture() = default;
};

struct D3D12Material
{
    ShaderResourceView* DiffuseSRV = nullptr;
	ShaderResourceView* AlphaSRV = nullptr;
};

}