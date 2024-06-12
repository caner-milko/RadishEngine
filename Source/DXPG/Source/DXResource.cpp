#include "DXResource.h"

namespace dxpg
{
DXTexture DXTexture::Create(ID3D12Device* device, std::wstring name, TextureCreateInfo const& info, D3D12_RESOURCE_STATES startState)
{
	// Create the texture
	auto desc = CD3DX12_RESOURCE_DESC::Tex2D(info.Format, info.Width, info.Height, info.DepthOrArraySize * (info.IsCubeMap ? 6 : 1), info.MipLevels, info.SampleDesc.Count, info.SampleDesc.Quality, info.Flags, info.Layout, info.Alignment);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ComPtr<ID3D12Resource> resource;

	device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		startState,
		info.ClearValue ? &*info.ClearValue : nullptr,
		IID_PPV_ARGS(&resource));

	return DXTexture(name, resource, startState, info);
}

std::unique_ptr<ShaderResourceView> DXTexture::CreateSRV(D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc)
{
	return ShaderResourceView::Create({ ResourceViewToDesc<ViewTypes::ShaderResourceView>{ srvDesc, Resource.Get() } });
}

std::unique_ptr<UnorderedAccessView> DXTexture::CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc)
{
	return UnorderedAccessView::Create({ ResourceViewToDesc<ViewTypes::UnorderedAccessView>{ uavDesc, Resource.Get() } });
}

std::unique_ptr<RenderTargetView> DXTexture::CreateRTV(D3D12_RENDER_TARGET_VIEW_DESC const* rtvDesc)
{
	return RenderTargetView::Create({ ResourceViewToDesc<ViewTypes::RenderTargetView>{ rtvDesc, Resource.Get() } });
}

std::unique_ptr<DepthStencilView> DXTexture::CreateDSV(D3D12_DEPTH_STENCIL_VIEW_DESC const* dsvDesc)
{
	return DepthStencilView::Create({ ResourceViewToDesc<ViewTypes::DepthStencilView>{ dsvDesc, Resource.Get() } });
}

DXBuffer DXBuffer::Create(ID3D12Device* device, std::wstring name, size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
{
	ComPtr<ID3D12Resource> resource;
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(heapType);
	device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		nullptr,
		IID_PPV_ARGS(&resource));
	return DXBuffer(name, resource, state, size);
}

std::unique_ptr<ShaderResourceView> DXBuffer::CreateSRV(size_t size, size_t offset, size_t stride, D3D12_BUFFER_SRV_FLAGS flags)
{
	size = size ? size : Size;
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Buffer.FirstElement = offset;
	desc.Buffer.NumElements = size;
	desc.Buffer.StructureByteStride = 0;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	return ShaderResourceView::Create({ ResourceViewToDesc<ViewTypes::ShaderResourceView>{ &desc, Resource.Get() } });
}

std::unique_ptr<UnorderedAccessView> DXBuffer::CreateUAV(size_t numElements, size_t stride, size_t firstElement, D3D12_BUFFER_UAV_FLAGS flags)
{
	if (stride == 0)
		stride = Size / numElements;
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = firstElement;
	desc.Buffer.NumElements = numElements;
	desc.Buffer.StructureByteStride = stride;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	return UnorderedAccessView::Create({ ResourceViewToDesc<ViewTypes::UnorderedAccessView>{ &desc, Resource.Get() } });
}

std::unique_ptr<ConstantBufferView> DXBuffer::CreateCBV(size_t size, size_t offset)
{
	size = size ? size : Size;
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = Resource->GetGPUVirtualAddress() + offset;
	desc.SizeInBytes = size;
	return ConstantBufferView::Create({ ResourceViewToDesc<ViewTypes::ConstantBufferView>{ &desc} });
}
void DXTexture::CreatePlacedSRV(DescriptorAllocationView alloc, D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc)
{
	alloc.Base->Heap->Device->CreateShaderResourceView(Resource.Get(), srvDesc, alloc.GetCPUHandle());
}
void DXTexture::CreatePlacedUAV(DescriptorAllocationView alloc,  D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc)
{
	alloc.Base->Heap->Device->CreateUnorderedAccessView(Resource.Get(), nullptr, uavDesc, alloc.GetCPUHandle());
}
void DXTexture::CreatePlacedRTV(DescriptorAllocationView alloc, D3D12_RENDER_TARGET_VIEW_DESC const* rtvDesc)
{
	alloc.Base->Heap->Device->CreateRenderTargetView(Resource.Get(), rtvDesc, alloc.GetCPUHandle());
}
void DXTexture::CreatePlacedDSV(DescriptorAllocationView alloc, D3D12_DEPTH_STENCIL_VIEW_DESC const* dsvDesc)
{
	alloc.Base->Heap->Device->CreateDepthStencilView(Resource.Get(), dsvDesc, alloc.GetCPUHandle());
}

}