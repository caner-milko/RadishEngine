#pragma once
#include "RadishCommon.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <span>
#include <unordered_map>
#include <optional>
#include <functional>
#include <queue>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include <wrl.h>
using Microsoft::WRL::ComPtr;
#include <directx/d3dx12.h>
#include <d3dcompiler.h>


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

namespace rad
{

using RadGraphicsCommandList = ID3D12GraphicsCommandList2;
using RadDevice = ID3D12Device2;
struct DescriptorHeap
{
	DescriptorHeap(RadDevice& device) : Device(device) {}
    static std::unique_ptr<DescriptorHeap> Create(D3D12_DESCRIPTOR_HEAP_DESC desc, RadDevice& device);

    Ref<RadDevice> Device;
    ComPtr<ID3D12DescriptorHeap> Heap;
    D3D12_DESCRIPTOR_HEAP_DESC Desc;
    uint32_t Increment;

    uint32_t Top = 0;

    uint32_t Allocate(uint32_t count = 1)
    {
        uint32_t old = Top;
        Top += count;
        assert(GetSize() >= Top);
        return old;
    }
    inline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = Heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * Increment;
        return handle;
    }
    inline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = Heap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += index * Increment;
        return handle;
    }
    uint32_t GetSize() const { return Desc.NumDescriptors; }
};

struct DescriptorAllocationView
{
    struct DescriptorAllocation* Base;
    uint32_t Offset;

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle();
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle();

    uint32_t GetIndex() const;
};

struct DescriptorAllocation
{
    static DescriptorAllocation Create(DescriptorHeap* heap, uint32_t size);
    static DescriptorAllocation CreatePreAllocated(DescriptorHeap* heap, uint32_t index, uint32_t size);
	
    DescriptorHeap* Heap;
	uint32_t Index;
	uint32_t Size;

    inline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t offset = 0)
	{
		return Heap->GetCPUHandle(Index + offset);
	}
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t offset = 0)
	{
		return Heap->GetGPUHandle(Index + offset);
	}

	inline DescriptorAllocationView GetView(uint32_t offset = 0)
	{
		return { this, offset };
	}

    DescriptorAllocation() = default;
};

inline D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocationView::GetCPUHandle()
{
	return Base->GetCPUHandle(Offset);
}

inline D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocationView::GetGPUHandle()
{
	return Base->GetGPUHandle(Offset);
}

inline uint32_t DescriptorAllocationView::GetIndex() const
{
	return Base->Index + Offset;
}

struct DescriptorHeapPage
{
    DescriptorHeap* Heap{};
    uint32_t Offset{};
    uint32_t Size{};
    uint32_t Top = 0;

    static std::unique_ptr<DescriptorHeapPage> Create(DescriptorHeap* heap, uint32_t size)
    {
		auto page = std::unique_ptr<DescriptorHeapPage>(new DescriptorHeapPage);
		page->Heap = heap;
		page->Size = size;
		page->Offset = heap->Allocate(size);
		return page;
	}

    DescriptorAllocation Allocate(uint32_t count)
    {
        uint32_t old = Top;
		Top += count;
		assert(Size >= Top);
		return DescriptorAllocation::CreatePreAllocated(Heap, old + Offset, count);
    }

    DescriptorAllocation CopyFrom(DescriptorAllocation* alloc)
	{
    	auto newAlloc = Allocate(alloc->Size);
        Heap->Device->CopyDescriptorsSimple(alloc->Size, newAlloc.GetCPUHandle(), alloc->GetCPUHandle(), Heap->Desc.Type);
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

    static std::unique_ptr<DescriptorHeapPageCollection> Create(D3D12_DESCRIPTOR_HEAP_DESC desc, RadDevice& device, uint32_t pageCount, uint32_t staticPageSize)
    {
		auto collection = std::make_unique<DescriptorHeapPageCollection>();
		collection->Heap = DescriptorHeap::Create(desc, device);
        uint32_t remSize = desc.NumDescriptors - staticPageSize;
        if (pageCount == 0)
            remSize = 0;
        else
            remSize = remSize - remSize % pageCount;
        collection->StaticPage = DescriptorHeapPage::Create(collection->Heap.get(), desc.NumDescriptors - remSize);
        for (uint32_t i = 0; i < pageCount; i++)
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

    DescriptorAllocation AllocateFromStatic(uint32_t count = 1)
    {
        return StaticPage->Allocate(count);
	}

};

template<bool CPU>
struct DescriptorHeapAllocator
{
    std::unordered_map<D3D12_DESCRIPTOR_HEAP_TYPE, std::unique_ptr<DescriptorHeapPageCollection>> Heaps;
	Ref<RadDevice> Device;

	DescriptorHeapAllocator(RadDevice& device) : Device(device) {}
    static std::unique_ptr<DescriptorHeapAllocator> Create(RadDevice& dev);

    void CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, uint32_t pageCount = 0, uint32_t staticPageSize = 0);

    DescriptorAllocation AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size = 1)
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
	const D3D12_SHADER_RESOURCE_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};
template<> struct ResourceViewToDesc<ViewTypes::UnorderedAccessView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};
template<> struct ResourceViewToDesc<ViewTypes::ConstantBufferView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	const D3D12_CONSTANT_BUFFER_VIEW_DESC* Desc;
};
template<> struct ResourceViewToDesc<ViewTypes::Sampler> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	const D3D12_SAMPLER_DESC* Desc;
};
template<> struct ResourceViewToDesc<ViewTypes::RenderTargetView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	const D3D12_RENDER_TARGET_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};
template<> struct ResourceViewToDesc<ViewTypes::DepthStencilView> {
	const static D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	const D3D12_DEPTH_STENCIL_VIEW_DESC* Desc;
	ID3D12Resource* Resource;
};

template<ViewTypes type>
struct ResourceView : DescriptorAllocation
{
    static ResourceView<type> Create(typename ResourceViewToDesc<type> descs)
    {
		return Create(std::span{ &descs, 1 });
	}
    static ResourceView<type> Create(std::span<typename ResourceViewToDesc<type>> descs);
};

using ShaderResourceView = ResourceView<ViewTypes::ShaderResourceView>;
using UnorderedAccessView = ResourceView<ViewTypes::UnorderedAccessView>;
using ConstantBufferView = ResourceView<ViewTypes::ConstantBufferView>;
using Sampler = ResourceView<ViewTypes::Sampler>;
using RenderTargetView = ResourceView<ViewTypes::RenderTargetView>;
using DepthStencilView = ResourceView<ViewTypes::DepthStencilView>;

struct TransitionVec : std::vector<D3D12_RESOURCE_BARRIER>
{
	using std::vector<D3D12_RESOURCE_BARRIER>::vector;
    TransitionVec(struct DXResource& res, D3D12_RESOURCE_STATES after)
    {
		Add(res, after);
    }

    TransitionVec& Add(struct DXResource& res, D3D12_RESOURCE_STATES after);
    TransitionVec& Add(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void Execute(RadGraphicsCommandList& cmdList);
};

struct CommandContext;


}