#include "DXHelpers.h"

#include "DXResource.h"

namespace dxpg
{
    std::unique_ptr<CPUDescriptorHeapAllocator> g_CPUDescriptorAllocator = nullptr;
    std::unique_ptr<GPUDescriptorHeapAllocator> g_GPUDescriptorAllocator = nullptr;

    std::unique_ptr<DescriptorHeap> DescriptorHeap::Create(D3D12_DESCRIPTOR_HEAP_DESC desc, ID3D12Device* device)
    {
        auto heap = std::make_unique<DescriptorHeap>();
        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap->Heap))))
			return nullptr;
        heap->Device = device;
        heap->Desc = desc;
        heap->Increment = device->GetDescriptorHandleIncrementSize(desc.Type);
        heap->Top = 0;
        return heap;
    }

    std::unique_ptr<DescriptorAllocation> DescriptorAllocation::Create(DescriptorHeap* heap, uint32_t size)
    {
        auto alloc = std::unique_ptr<DescriptorAllocation>(new DescriptorAllocation());
        alloc->Heap = heap;
        alloc->Index = heap->Allocate(size);
        alloc->Size = size;
        return alloc;
    }

    std::unique_ptr<DescriptorAllocation> DescriptorAllocation::CreatePreAllocated(DescriptorHeap* heap, size_t index, size_t size)
    {
        auto alloc = std::unique_ptr<DescriptorAllocation>(new DescriptorAllocation());
        alloc->Heap = heap;
        alloc->Index = index;
        alloc->Size = size;
    	return alloc;
    }


    template<bool CPU>
    std::unique_ptr<DescriptorHeapAllocator<CPU>> DescriptorHeapAllocator<CPU>::Create(ID3D12Device* dev)
    {
        auto allocator = std::make_unique<DescriptorHeapAllocator>();
        allocator->Device = dev;
        //D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        //desc.NumDescriptors = numDescriptors;
        //if constexpr (CPU)
        //{

        //    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
        //    {
        //        desc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
        //        allocator->Heaps[(D3D12_DESCRIPTOR_HEAP_TYPE)i] = DescriptorHeapPageCollection::Create(desc, dev, pageCount, staticPageSize);
        //    }
        //}
        //else
        //{
        //    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        //    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        //    allocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = DescriptorHeapPageCollection::Create(desc, dev, pageCount, staticPageSize);
        //    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        //    allocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = DescriptorHeapPageCollection::Create(desc, dev, pageCount, staticPageSize);
        //}

        return allocator;
    }
    template<bool CPU>
    void DescriptorHeapAllocator<CPU>::CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors, uint32_t pageCount, size_t staticPageSize)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = numDescriptors;
		desc.Type = type;
		if constexpr (!CPU)
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		Heaps[type] = DescriptorHeapPageCollection::Create(desc, Device, pageCount, staticPageSize);
    }
    template class DescriptorHeapAllocator<true>;
    template class DescriptorHeapAllocator<false>;


    template<ViewTypes type>
    std::unique_ptr<ResourceView<type>> ResourceView<type>::Create(std::span<typename ResourceViewToDesc<type>> descs)
    {
        using ViewToDesc = ResourceViewToDesc<type>;
		auto* device = g_CPUDescriptorAllocator->Device;
        auto* view = g_CPUDescriptorAllocator->AllocateFromStatic(ViewToDesc::HeapType, descs.size()).release();
        for (size_t i = 0; i < descs.size(); i++)
        {
			auto& desc = descs[i];
            if constexpr (type == ViewTypes::ShaderResourceView)
                device->CreateShaderResourceView(desc.Resource, desc.Desc, view->GetCPUHandle(i));
            else if constexpr (type == ViewTypes::UnorderedAccessView)
            {
                device->CreateUnorderedAccessView(desc.Resource, nullptr, desc.Desc, view->GetCPUHandle(i));
            }
            else if constexpr (type == ViewTypes::ConstantBufferView)
                device->CreateConstantBufferView(desc.Desc, view->GetCPUHandle(i));
            else if constexpr (type == ViewTypes::Sampler)
                device->CreateSampler(desc.Desc, view->GetCPUHandle(i));
            else if constexpr (type == ViewTypes::RenderTargetView)
                device->CreateRenderTargetView(desc.Resource, desc.Desc, view->GetCPUHandle(i));
            else if constexpr (type == ViewTypes::DepthStencilView)
                device->CreateDepthStencilView(desc.Resource, desc.Desc, view->GetCPUHandle(i));
            else
            {
                assert(false);
            }
        }
        return std::unique_ptr<ResourceView<type>>(reinterpret_cast<ResourceView<type>*>(view));
    }
    template class ResourceView<ViewTypes::ShaderResourceView>;
    template class ResourceView<ViewTypes::UnorderedAccessView>;
    template class ResourceView<ViewTypes::ConstantBufferView>;
    template class ResourceView<ViewTypes::Sampler>;
    template class ResourceView<ViewTypes::RenderTargetView>;
    template class ResourceView<ViewTypes::DepthStencilView>;



    TransitionVec& TransitionVec::Add(DXResource& res, D3D12_RESOURCE_STATES after)
    {
		if (res.State != after)
			push_back(res.Transition(after));
        return *this;
    }

    TransitionVec& TransitionVec::Add(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
		if (before != after)
			push_back(D3D12_RESOURCE_BARRIER{ .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, .Transition = { res, 0, before, after } });
		return *this;
    }

    void TransitionVec::Execute(ID3D12GraphicsCommandList* cmdList)
    {
        if (size() == 0)
            return;
		cmdList->ResourceBarrier(size(), data());
    }


}