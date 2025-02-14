#include "ResourcePool.h"
#include "Renderer.h"

namespace rad
{
template <typename T, auto... Desc> bool CompareDesc(const T& Desc1, const T& Desc2)
{
	return ((Desc1.*Desc == Desc2.*Desc) && ...);
}

bool ShaderResourceViewDesc::operator==(const ShaderResourceViewDesc& Other) const
{
	if (Other.Format != Format || Other.ViewDimension != ViewDimension ||
		Other.Shader4ComponentMapping != Shader4ComponentMapping)
		return false;

	switch (ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_SRV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		auto& otherDesc = Other.ViewName;                                                                              \
		using Desc = std::decay_t<decltype(desc)>;                                                                     \
		return CompareDesc<Desc, __VA_ARGS__>(desc, otherDesc);                                                        \
	}
		COMPARE_VIEW(BUFFER, Buffer, &Desc::FirstElement, &Desc::NumElements, &Desc::StructureByteStride, &Desc::Flags)
		COMPARE_VIEW(TEXTURE1D, Texture1D, &Desc::MostDetailedMip, &Desc::MipLevels, &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(TEXTURE1DARRAY, Texture1DArray, &Desc::MostDetailedMip, &Desc::MipLevels, &Desc::FirstArraySlice,
					 &Desc::ArraySize, &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(TEXTURE2D, Texture2D, &Desc::MostDetailedMip, &Desc::MipLevels, &Desc::PlaneSlice,
					 &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(TEXTURE2DARRAY, Texture2DArray, &Desc::MostDetailedMip, &Desc::MipLevels, &Desc::FirstArraySlice,
					 &Desc::ArraySize, &Desc::PlaneSlice, &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(TEXTURE2DMS, Texture2DMS, &Desc::UnusedField_NothingToDefine)
		COMPARE_VIEW(TEXTURE2DMSARRAY, Texture2DMSArray, &Desc::FirstArraySlice, &Desc::ArraySize)
		COMPARE_VIEW(TEXTURE3D, Texture3D, &Desc::MostDetailedMip, &Desc::MipLevels, &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(TEXTURECUBE, TextureCube, &Desc::MostDetailedMip, &Desc::MipLevels, &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(TEXTURECUBEARRAY, TextureCubeArray, &Desc::MostDetailedMip, &Desc::MipLevels,
					 &Desc::First2DArrayFace, &Desc::NumCubes, &Desc::ResourceMinLODClamp)
		COMPARE_VIEW(RAYTRACING_ACCELERATION_STRUCTURE, RaytracingAccelerationStructure, &Desc::Location)
	default:
		assert(false);
		return false;
#undef COMPARE_VIEW
	}
}
size_t ShaderResourceViewDesc::Hash() const
{
	size_t hash = HashCombine(Format, ViewDimension, Shader4ComponentMapping);
	switch (ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_SRV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		hash = HashCombine(hash, __VA_ARGS__);                                                                         \
		break;                                                                                                         \
	}
		HASH_VIEW(BUFFER, Buffer, desc.FirstElement, desc.NumElements, desc.StructureByteStride, desc.Flags)
		HASH_VIEW(TEXTURE1D, Texture1D, desc.MostDetailedMip, desc.MipLevels, desc.ResourceMinLODClamp)
		HASH_VIEW(TEXTURE1DARRAY, Texture1DArray, desc.MostDetailedMip, desc.MipLevels, desc.FirstArraySlice,
				  desc.ArraySize, desc.ResourceMinLODClamp)
		HASH_VIEW(TEXTURE2D, Texture2D, desc.MostDetailedMip, desc.MipLevels, desc.PlaneSlice, desc.ResourceMinLODClamp)
		HASH_VIEW(TEXTURE2DARRAY, Texture2DArray, desc.MostDetailedMip, desc.MipLevels, desc.FirstArraySlice,
				  desc.ArraySize, desc.PlaneSlice, desc.ResourceMinLODClamp)
		HASH_VIEW(TEXTURE2DMS, Texture2DMS, desc.UnusedField_NothingToDefine)
		HASH_VIEW(TEXTURE2DMSARRAY, Texture2DMSArray, desc.FirstArraySlice, desc.ArraySize)
		HASH_VIEW(TEXTURE3D, Texture3D, desc.MostDetailedMip, desc.MipLevels, desc.ResourceMinLODClamp)
		HASH_VIEW(TEXTURECUBE, TextureCube, desc.MostDetailedMip, desc.MipLevels, desc.ResourceMinLODClamp)
		HASH_VIEW(TEXTURECUBEARRAY, TextureCubeArray, desc.MostDetailedMip, desc.MipLevels, desc.First2DArrayFace,
				  desc.NumCubes, desc.ResourceMinLODClamp)
		HASH_VIEW(RAYTRACING_ACCELERATION_STRUCTURE, RaytracingAccelerationStructure, desc.Location)
	default:
		assert(false);
		break;
#undef HASH_VIEW
	}
	return hash;
}

bool UnorderedAccessViewDesc::operator==(const UnorderedAccessViewDesc& Other) const
{
	if (Other.Format != Format || Other.ViewDimension != ViewDimension)
		return false;
	switch (ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_UAV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		auto& otherDesc = Other.ViewName;                                                                              \
		using Desc = std::decay_t<decltype(desc)>;                                                                     \
		return CompareDesc<Desc, __VA_ARGS__>(desc, otherDesc);                                                        \
	}
		COMPARE_VIEW(BUFFER, Buffer, &Desc::FirstElement, &Desc::NumElements, &Desc::StructureByteStride,
					 &Desc::CounterOffsetInBytes, &Desc::Flags)
		COMPARE_VIEW(TEXTURE1D, Texture1D, &Desc::MipSlice)
		COMPARE_VIEW(TEXTURE1DARRAY, Texture1DArray, &Desc::MipSlice, &Desc::FirstArraySlice, &Desc::ArraySize)
		COMPARE_VIEW(TEXTURE2D, Texture2D, &Desc::MipSlice, &Desc::PlaneSlice)
		COMPARE_VIEW(TEXTURE2DARRAY, Texture2DArray, &Desc::MipSlice, &Desc::FirstArraySlice, &Desc::ArraySize,
					 &Desc::PlaneSlice)
		COMPARE_VIEW(TEXTURE3D, Texture3D, &Desc::MipSlice, &Desc::FirstWSlice, &Desc::WSize)
	default:
		assert(false);
		return false;
#undef COMPARE_VIEW
	}
}
size_t UnorderedAccessViewDesc::Hash() const
{
	size_t hash = HashCombine(Format, ViewDimension);
	switch (ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_UAV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		hash = HashCombine(hash, __VA_ARGS__);                                                                         \
		break;                                                                                                         \
	}
		HASH_VIEW(BUFFER, Buffer, desc.FirstElement, desc.NumElements, desc.StructureByteStride,
				  desc.CounterOffsetInBytes, desc.Flags)
		HASH_VIEW(TEXTURE1D, Texture1D, desc.MipSlice)
		HASH_VIEW(TEXTURE1DARRAY, Texture1DArray, desc.MipSlice, desc.FirstArraySlice, desc.ArraySize)
		HASH_VIEW(TEXTURE2D, Texture2D, desc.MipSlice, desc.PlaneSlice)
		HASH_VIEW(TEXTURE2DARRAY, Texture2DArray, desc.MipSlice, desc.FirstArraySlice, desc.ArraySize, desc.PlaneSlice)
		HASH_VIEW(TEXTURE2DMS, Texture2DMS, desc.UnusedField_NothingToDefine)
		HASH_VIEW(TEXTURE3D, Texture3D, desc.MipSlice, desc.FirstWSlice, desc.WSize)
	default:
		assert(false);
		break;
#undef HASH_VIEW
	}
	return hash;
}

bool ConstantBufferViewDesc::operator==(const ConstantBufferViewDesc& Other) const
{
	return BufferLocation == Other.BufferLocation && SizeInBytes == Other.SizeInBytes;
}
size_t ConstantBufferViewDesc::Hash() const
{
	return HashCombine(BufferLocation, SizeInBytes);
}

bool RenderTargetViewDesc::operator==(const RenderTargetViewDesc& Other) const
{
	if (Other.Format != Format || Other.ViewDimension != ViewDimension)
		return false;
	switch (ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_RTV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		auto& otherDesc = Other.ViewName;                                                                              \
		using Desc = std::decay_t<decltype(desc)>;                                                                     \
		return CompareDesc<Desc, __VA_ARGS__>(desc, otherDesc);                                                        \
	}
		COMPARE_VIEW(BUFFER, Buffer, &Desc::FirstElement)
		COMPARE_VIEW(TEXTURE1D, Texture1D, &Desc::MipSlice)
		COMPARE_VIEW(TEXTURE1DARRAY, Texture1DArray, &Desc::MipSlice, &Desc::FirstArraySlice, &Desc::ArraySize)
		COMPARE_VIEW(TEXTURE2D, Texture2D, &Desc::MipSlice, &Desc::PlaneSlice)
		COMPARE_VIEW(TEXTURE2DARRAY, Texture2DArray, &Desc::MipSlice, &Desc::FirstArraySlice, &Desc::ArraySize,
					 &Desc::PlaneSlice)
		COMPARE_VIEW(TEXTURE2DMS, Texture2DMS, &Desc::UnusedField_NothingToDefine)
		COMPARE_VIEW(TEXTURE2DMSARRAY, Texture2DMSArray, &Desc::FirstArraySlice, &Desc::ArraySize)
		COMPARE_VIEW(TEXTURE3D, Texture3D, &Desc::MipSlice, &Desc::FirstWSlice, &Desc::WSize)
	default:
		assert(false);
		return false;
#undef COMPARE_VIEW
	}
}
size_t RenderTargetViewDesc::Hash() const
{
	size_t hash = HashCombine(Format, ViewDimension);
	switch (ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_RTV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		hash = HashCombine(hash, __VA_ARGS__);                                                                         \
		break;                                                                                                         \
	}
		HASH_VIEW(BUFFER, Buffer, desc.FirstElement)
		HASH_VIEW(TEXTURE1D, Texture1D, desc.MipSlice)
		HASH_VIEW(TEXTURE1DARRAY, Texture1DArray, desc.MipSlice, desc.FirstArraySlice, desc.ArraySize)
		HASH_VIEW(TEXTURE2D, Texture2D, desc.MipSlice, desc.PlaneSlice)
		HASH_VIEW(TEXTURE2DARRAY, Texture2DArray, desc.MipSlice, desc.FirstArraySlice, desc.ArraySize, desc.PlaneSlice)
		HASH_VIEW(TEXTURE2DMS, Texture2DMS, desc.UnusedField_NothingToDefine)
		HASH_VIEW(TEXTURE2DMSARRAY, Texture2DMSArray, desc.FirstArraySlice, desc.ArraySize)
		HASH_VIEW(TEXTURE3D, Texture3D, desc.MipSlice, desc.FirstWSlice, desc.WSize)
	default:
		assert(false);
		break;
#undef HASH_VIEW
	}
	return hash;
}

bool DepthStencilViewDesc::operator==(const DepthStencilViewDesc& Other) const
{
	if (Other.Format != Format || Other.ViewDimension != ViewDimension)
		return false;
	switch (ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_DSV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		auto& otherDesc = Other.ViewName;                                                                              \
		using Desc = std::decay_t<decltype(desc)>;                                                                     \
		return CompareDesc<Desc, __VA_ARGS__>(desc, otherDesc);                                                        \
	}
		COMPARE_VIEW(TEXTURE1D, Texture1D, &Desc::MipSlice)
		COMPARE_VIEW(TEXTURE1DARRAY, Texture1DArray, &Desc::MipSlice, &Desc::FirstArraySlice, &Desc::ArraySize)
		COMPARE_VIEW(TEXTURE2D, Texture2D, &Desc::MipSlice)
		COMPARE_VIEW(TEXTURE2DARRAY, Texture2DArray, &Desc::MipSlice, &Desc::FirstArraySlice, &Desc::ArraySize)
		COMPARE_VIEW(TEXTURE2DMS, Texture2DMS, &Desc::UnusedField_NothingToDefine)
		COMPARE_VIEW(TEXTURE2DMSARRAY, Texture2DMSArray, &Desc::FirstArraySlice, &Desc::ArraySize)
	default:
		assert(false);
		return false;
#undef COMPARE_VIEW
	}
}
size_t DepthStencilViewDesc::Hash() const
{
	size_t hash = HashCombine(Format, ViewDimension);
	switch (ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_DSV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = ViewName;                                                                                         \
		hash = HashCombine(hash, __VA_ARGS__);                                                                         \
		break;                                                                                                         \
	}
		HASH_VIEW(TEXTURE1D, Texture1D, desc.MipSlice)
		HASH_VIEW(TEXTURE1DARRAY, Texture1DArray, desc.MipSlice, desc.FirstArraySlice, desc.ArraySize)
		HASH_VIEW(TEXTURE2D, Texture2D, desc.MipSlice)
		HASH_VIEW(TEXTURE2DARRAY, Texture2DArray, desc.MipSlice, desc.FirstArraySlice, desc.ArraySize)
		HASH_VIEW(TEXTURE2DMS, Texture2DMS, desc.UnusedField_NothingToDefine)
		HASH_VIEW(TEXTURE2DMSARRAY, Texture2DMSArray, desc.FirstArraySlice, desc.ArraySize)
	default:
		assert(false);
		break;
#undef HASH_VIEW
	}
	return hash;
}

bool VertexBufferViewDesc::operator==(const VertexBufferViewDesc& Other) const
{
	return BufferLocation == Other.BufferLocation && SizeInBytes == Other.SizeInBytes &&
		   StrideInBytes == Other.StrideInBytes;
}
size_t VertexBufferViewDesc::Hash() const
{
	return HashCombine(BufferLocation, SizeInBytes, StrideInBytes);
}

bool IndexBufferViewDesc::operator==(const IndexBufferViewDesc& Other) const
{
	return BufferLocation == Other.BufferLocation && SizeInBytes == Other.SizeInBytes && Format == Other.Format;
}
size_t IndexBufferViewDesc::Hash() const
{
	return HashCombine(BufferLocation, SizeInBytes, Format);
}

bool ResourceCreateInfo::operator==(const ResourceCreateInfo& Other) const
{
#define COMPARE_FIELDS(Field, ...)                                                                                     \
	{                                                                                                                  \
		using T = decltype(Field);                                                                                     \
		if (!CompareDesc<T, __VA_ARGS__>(Field, Other.Field))                                                          \
			return false;                                                                                              \
	}
	COMPARE_FIELDS(Desc, &T::Dimension, &T::Alignment, &T::Width, &T::Height, &T::DepthOrArraySize, &T::MipLevels,
				   &T::Format, &T::Layout, &T::Flags);
	if (Desc.SampleDesc.Count != Other.Desc.SampleDesc.Count ||
		Desc.SampleDesc.Quality != Other.Desc.SampleDesc.Quality)
		return false;

	if (Flags != Other.Flags)
		return false;

	COMPARE_FIELDS(HeapDesc, &T::SizeInBytes, &T::Alignment, &T::Flags);
	COMPARE_FIELDS(HeapDesc.Properties, &T::Type, &T::CPUPageProperty, &T::MemoryPoolPreference, &T::CreationNodeMask,
				   &T::VisibleNodeMask);

	if (HeapFlags != Other.HeapFlags)
		return false;
	return true;
}
size_t ResourceCreateInfo::Hash() const
{
	return HashCombine(Desc.Dimension, Desc.Alignment, Desc.Width, Desc.Height, Desc.DepthOrArraySize, Desc.MipLevels,
					   Desc.Format, Desc.Layout, Desc.Flags, Desc.SampleDesc.Count, Desc.SampleDesc.Quality, Flags,
					   HeapDesc.SizeInBytes, HeapDesc.Alignment, HeapDesc.Flags, HeapDesc.Properties.Type,
					   HeapDesc.Properties.CPUPageProperty, HeapDesc.Properties.MemoryPoolPreference,
					   HeapDesc.Properties.CreationNodeMask, HeapDesc.Properties.VisibleNodeMask, HeapFlags);
}
ResourcePool::ResourcePool(rad::Renderer& renderer) : Renderer(renderer) {}

PoolResourceView ResourcePool::GetResource(const ResourceCreateInfo& createInfo, std::string acquireName)
{
	if (auto it = FreeResources.find(createInfo); it != FreeResources.end())
	{
		if (!it->second.empty())
		{
			OwnedResource& resource = it->second.back();
			FreeResources.erase(it);
			resource.AcquiredName = std::move(acquireName);
			return PoolResourceView(resource);
		}
	}
	// Create ID3D12Resource from CreateInfo
	ComPtr<ID3D12Resource> resource;

	auto& resInfo = AddResourceInfo(*resource.Get(), createInfo, D3D12_RESOURCE_STATE_COMMON);
	auto& ownedResource = OwnedResources[createInfo].emplace_back(OwnedResource(std::move(resource), resInfo));
	ownedResource.AcquiredName = std::move(acquireName);
	return PoolResourceView(ownedResource);
}

PoolResourceView ResourcePool::AddExternalResource(ID3D12Resource& resource, std::string name,
												   const ResourceCreateInfo& createInfo,
												   D3D12_RESOURCE_STATES initialState)
{
	auto& resInfo = AddResourceInfo(resource, createInfo, initialState);
	return PoolResourceView(
		ExternalResources.insert_or_assign(resource, ExternalResource(resource, name, resInfo)).first->second);
}

void ResourcePool::FreeResource(OwnedResource& resource)
{
	FreeResources[resource->CreateInfo].emplace_back(resource);
}

ResourcePool::Resource& ResourcePool::AddResourceInfo(ID3D12Resource& resource, const ResourceCreateInfo& createInfo,
													  D3D12_RESOURCE_STATES initialState)
{
	return Resources.insert_or_assign(resource, Resource{createInfo, resource, initialState}).first->second;
}
ResourceDescriptor& ResourcePool::GetDescriptor(PoolResourceView& resourceView, const DescriptorDesc& desc)
{
	assert(Resources.contains(resourceView->DXRes) && "Resource not found in pool");
	auto& resource = Resources.at(resourceView->DXRes);
	auto& descriptors = resource.Descriptors;
	if (auto it = descriptors.find(desc); it != descriptors.end())
		return it->second;

	// Create descriptor
	if (auto* gpuDesc = std::get_if<GPUDescriptorDesc>(&desc))
	{
		auto alloc = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		if (auto* srvDesc = std::get_if<ShaderResourceViewDesc>(gpuDesc))
			Renderer.GetDevice().CreateShaderResourceView(&resourceView->DXRes, srvDesc, alloc.GetCPUHandle());
		else if (auto* uavDesc = std::get_if<UnorderedAccessViewDesc>(gpuDesc))
			Renderer.GetDevice().CreateUnorderedAccessView(&resourceView->DXRes, nullptr, uavDesc,
														   alloc.GetCPUHandle());
		else if (auto* cbvDesc = std::get_if<ConstantBufferViewDesc>(gpuDesc))
			Renderer.GetDevice().CreateConstantBufferView(cbvDesc, alloc.GetCPUHandle());
		else
			assert(false && "Invalid descriptor type");
		return descriptors.emplace(desc, ResourceDescriptor{alloc}).first->second;
	}
	else if (auto* cpuDesc = std::get_if<CPUDescriptorDesc>(&desc))
	{
		CPUResourceDescriptor resoureDesc;
		if (auto* srvDesc = std::get_if<ShaderResourceViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			Renderer.GetDevice().CreateShaderResourceView(&resourceView->DXRes, srvDesc, alloc.GetCPUHandle());
			resoureDesc.ViewDesc = alloc;
		}
		else if (auto* uavDesc = std::get_if<UnorderedAccessViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			Renderer.GetDevice().CreateUnorderedAccessView(&resourceView->DXRes, nullptr, uavDesc,
														   alloc.GetCPUHandle());
			resoureDesc.ViewDesc = alloc;
		}
		else if (auto* cbvDesc = std::get_if<ConstantBufferViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			Renderer.GetDevice().CreateConstantBufferView(cbvDesc, alloc.GetCPUHandle());
			resoureDesc.ViewDesc = alloc;
		}
		else if (auto* rtvDesc = std::get_if<RenderTargetViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
			Renderer.GetDevice().CreateRenderTargetView(&resourceView->DXRes, rtvDesc, alloc.GetCPUHandle());
			resoureDesc.ViewDesc = alloc;
		}
		else if (auto* dsvDesc = std::get_if<DepthStencilViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
			Renderer.GetDevice().CreateDepthStencilView(&resourceView->DXRes, dsvDesc, alloc.GetCPUHandle());
			resoureDesc.ViewDesc = alloc;
		}
		else if (auto* vbvDesc = std::get_if<VertexBufferViewDesc>(cpuDesc))
			resoureDesc.ViewDesc = *vbvDesc;
		else if (auto* ibvDesc = std::get_if<IndexBufferViewDesc>(cpuDesc))
			resoureDesc.ViewDesc = *ibvDesc;
		else
			assert(false && "Invalid descriptor type");

		return descriptors.emplace(desc, ResourceDescriptor{resoureDesc}).first->second;
	}
	else
	{
		assert(false && "Invalid descriptor type");
		return descriptors.emplace(desc, ResourceDescriptor{}).first->second;
	}
}
} // namespace rad