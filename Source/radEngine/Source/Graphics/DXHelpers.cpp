#include "DXHelpers.h"

#include "DXResource.h"

namespace rad
{

std::unique_ptr<CPUDescriptorHeapAllocator> g_CPUDescriptorAllocator = nullptr;
std::unique_ptr<GPUDescriptorHeapAllocator> g_GPUDescriptorAllocator = nullptr;

std::unique_ptr<DescriptorHeap> DescriptorHeap::Create(D3D12_DESCRIPTOR_HEAP_DESC desc, RadDevice& device)
{
	auto heap = std::make_unique<DescriptorHeap>(device);
	if (FAILED(device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap->Heap))))
		return nullptr;
	heap->Desc = desc;
	heap->Increment = device.GetDescriptorHandleIncrementSize(desc.Type);
	heap->Top = 0;
	return heap;
}

DescriptorAllocation DescriptorAllocation::Create(DescriptorHeap* heap, uint32_t size)
{
	DescriptorAllocation alloc{};
	alloc.Heap = heap;
	alloc.Index = heap->Allocate(size);
	alloc.Size = size;
	return alloc;
}

DescriptorAllocation DescriptorAllocation::CreatePreAllocated(DescriptorHeap* heap, uint32_t index, uint32_t size)
{
	DescriptorAllocation alloc{};
	alloc.Heap = heap;
	alloc.Index = index;
	alloc.Size = size;
	return alloc;
}


template<bool CPU>
std::unique_ptr<DescriptorHeapAllocator<CPU>> DescriptorHeapAllocator<CPU>::Create(RadDevice& device)
{
	auto allocator = std::make_unique<DescriptorHeapAllocator>(device);
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
void DescriptorHeapAllocator<CPU>::CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, uint32_t pageCount, uint32_t staticPageSize)
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
ResourceView<type> ResourceView<type>::Create(std::span<typename ResourceViewToDesc<type>> descs)
{
	using ViewToDesc = ResourceViewToDesc<type>;
	auto& device = g_CPUDescriptorAllocator->Device;
	auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(ViewToDesc::HeapType, descs.size());
	for (size_t i = 0; i < descs.size(); i++)
	{
		auto& desc = descs[i];
		if constexpr (type == ViewTypes::ShaderResourceView)
			device->CreateShaderResourceView(desc.Resource, desc.Desc, alloc.GetCPUHandle(i));
		else if constexpr (type == ViewTypes::UnorderedAccessView)
		{
			device->CreateUnorderedAccessView(desc.Resource, nullptr, desc.Desc, alloc.GetCPUHandle(i));
		}
		else if constexpr (type == ViewTypes::ConstantBufferView)
			device->CreateConstantBufferView(desc.Desc, alloc.GetCPUHandle(i));
		else if constexpr (type == ViewTypes::Sampler)
			device->CreateSampler(desc.Desc, alloc.GetCPUHandle(i));
		else if constexpr (type == ViewTypes::RenderTargetView)
			device->CreateRenderTargetView(desc.Resource, desc.Desc, alloc.GetCPUHandle(i));
		else if constexpr (type == ViewTypes::DepthStencilView)
			device->CreateDepthStencilView(desc.Resource, desc.Desc, alloc.GetCPUHandle(i));
		else
		{
			assert(false);
		}
	}
	return static_cast<ResourceView<type>>(alloc);
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

void TransitionVec::Execute(RadGraphicsCommandList& cmdList)
{
	if (size() == 0)
		return;
	cmdList.ResourceBarrier(size(), data());
}


}