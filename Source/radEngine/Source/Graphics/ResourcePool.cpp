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

	COMPARE_FIELDS(HeapProps, &T::Type, &T::CPUPageProperty, &T::MemoryPoolPreference, &T::CreationNodeMask,
				   &T::VisibleNodeMask);

	if (HeapFlags != Other.HeapFlags)
		return false;
	return true;
}
size_t ResourceCreateInfo::Hash() const
{
	return HashCombine(Desc.Dimension, Desc.Alignment, Desc.Width, Desc.Height, Desc.DepthOrArraySize, Desc.MipLevels,
					   Desc.Format, Desc.Layout, Desc.Flags, Desc.SampleDesc.Count, Desc.SampleDesc.Quality,
					   HeapProps.CPUPageProperty, HeapProps.MemoryPoolPreference, HeapProps.CreationNodeMask,
					   HeapProps.VisibleNodeMask, HeapFlags);
}
ResourcePool::ResourcePool(rad::Renderer& renderer) : Renderer(renderer) {}

ResourcePool::OwnedResource& ResourcePool::GetResource(const ResourceCreateInfo& createInfo, std::string acquireName)
{
	if (auto it = FreeResources.find(createInfo); it != FreeResources.end())
	{
		if (!it->second.empty())
		{
			OwnedResource& resource = it->second.back();
			FreeResources.erase(it);
			resource.AcquiredName = std::move(acquireName);
			;
			resource->DXRes->SetName(s2ws(*resource.AcquiredName).c_str());
			return resource;
		}
	}
	// Create ID3D12Resource from CreateInfo
	ComPtr<ID3D12Resource> resource;

	D3D12_CLEAR_VALUE clearValue = {.Format = createInfo.Desc.Format};
	switch (createInfo.Desc.Format)
	{
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_D16_UNORM:
		clearValue.DepthStencil.Depth = createInfo.ClearValue[0];
		clearValue.DepthStencil.Stencil = createInfo.ClearValue[1];
		break;
	default:
		memcpy(clearValue.Color, createInfo.ClearValue.data(), sizeof(clearValue.Color));
		break;
	}

	Renderer.GetDevice().CreateCommittedResource(&createInfo.HeapProps, createInfo.HeapFlags, &createInfo.Desc,
												 D3D12_RESOURCE_STATE_COMMON, &clearValue,
												 IID_PPV_ARGS(&resource));

	auto& resInfo = AddResourceInfo(*resource.Get(), createInfo, D3D12_RESOURCE_STATE_COMMON);
	auto& ownedResource = OwnedResources[createInfo].emplace_back(OwnedResource(std::move(resource), resInfo));
	ownedResource.AcquiredName = std::move(acquireName);
	ownedResource.DXRes->SetName(s2ws(*ownedResource.AcquiredName).c_str());
	return ownedResource;
}

ResourcePool::ExternalResource& ResourcePool::AddExternalResource(ID3D12Resource& resource, std::string name,
																  const ResourceCreateInfo& createInfo,
																  D3D12_RESOURCE_STATES initialState)
{
	auto& resInfo = AddResourceInfo(resource, createInfo, initialState);
	return ExternalResources.insert_or_assign(resource, ExternalResource(resource, name, resInfo)).first->second;
}
void ResourcePool::RemoveExternalResource(ResourcePool::ExternalResource& extRes)
{
	ExternalResources.erase(extRes.DXRes);
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
ResourceDescriptor& ResourcePool::GetDescriptor(const PoolResourceView& resourceView, const DescriptorDesc& desc)
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
			resoureDesc = alloc;
		}
		else if (auto* uavDesc = std::get_if<UnorderedAccessViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			Renderer.GetDevice().CreateUnorderedAccessView(&resourceView->DXRes, nullptr, uavDesc,
														   alloc.GetCPUHandle());
			resoureDesc = alloc;
		}
		else if (auto* cbvDesc = std::get_if<ConstantBufferViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			Renderer.GetDevice().CreateConstantBufferView(cbvDesc, alloc.GetCPUHandle());
			resoureDesc = alloc;
		}
		else if (auto* rtvDesc = std::get_if<RenderTargetViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
			Renderer.GetDevice().CreateRenderTargetView(&resourceView->DXRes, rtvDesc, alloc.GetCPUHandle());
			resoureDesc = alloc;
		}
		else if (auto* dsvDesc = std::get_if<DepthStencilViewDesc>(cpuDesc))
		{
			auto alloc = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
			Renderer.GetDevice().CreateDepthStencilView(&resourceView->DXRes, dsvDesc, alloc.GetCPUHandle());
			resoureDesc = alloc;
		}
		else if (auto* vbvDesc = std::get_if<VertexBufferViewDesc>(cpuDesc))
			resoureDesc = *vbvDesc;
		else if (auto* ibvDesc = std::get_if<IndexBufferViewDesc>(cpuDesc))
			resoureDesc = *ibvDesc;
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

PoolResourceView ResourcePool::OwnedResource::AsView()
{
	return PoolResourceView(*this);
}
PoolResourceView ResourcePool::ExternalResource::AsView()
{
	return PoolResourceView(*this);
}

D3D12_RESOURCE_FLAGS ResourceCreateHelper::ToResourceFlags(ResourcePresetFlags flags)
{
	D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;
	if (!!(flags & ResourcePresetFlags::RenderTarget))
		resFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (!!(flags & ResourcePresetFlags::DepthStencil))
		resFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (!(flags & ResourcePresetFlags::ShaderResource) && !!(flags & ResourcePresetFlags::DepthStencil))
		resFlags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	if (!!(flags & ResourcePresetFlags::UnorderedAccess))
		resFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (!!(flags & ResourcePresetFlags::MipMaps))
		resFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	return resFlags;
}

D3D12_HEAP_FLAGS ResourceCreateHelper::ToHeapFlags(ResourcePresetFlags flags, PresetType type)
{
	D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	// if (!(flags & ResourcePresetFlags::RenderTarget) || !(flags & ResourcePresetFlags::DepthStencil))
	//	heapFlags |= D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
	// if (!(flags & ResourcePresetFlags::ShaderResource) || !(flags & ResourcePresetFlags::UnorderedAccess))
	//	heapFlags |= D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
	// if (type != PresetType::Buffer)
	//	heapFlags |= D3D12_HEAP_FLAG_DENY_BUFFERS;
	return heapFlags;
}

D3D12_HEAP_PROPERTIES ResourceCreateHelper::ToHeapProps(ResourcePresetFlags flags)
{
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	if (!!(flags & ResourcePresetFlags::UploadResource))
		heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	return heapProps;
}

ResourceCreateInfo ResourceCreateHelper::Buffer(uint64_t size, ResourcePresetFlags flags, BufferDetails details)
{
	ResourceCreateInfo createInfo{};
	createInfo.Desc = CD3DX12_RESOURCE_DESC::Buffer(size, ToResourceFlags(flags) | details.DetailedFlags);
	createInfo.HeapFlags = ToHeapFlags(flags, PresetType::Buffer) | details.HeapFlags;
	createInfo.HeapProps = details.Heap ? *details.Heap : ToHeapProps(flags);
	return createInfo;
}

ResourceCreateInfo ResourceCreateHelper::Texture2D(uint32_t width, uint32_t height, DXGI_FORMAT format,
												   ResourcePresetFlags flags,
												   TextureDetails details)
{
	ResourceCreateInfo createInfo{};
	createInfo.Desc =
		CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, !!(flags & ResourcePresetFlags::MipMaps) ? 0 : 1, 1, 0,
									 ToResourceFlags(flags) | details.DetailedFlags);
	createInfo.HeapFlags = ToHeapFlags(flags, PresetType::Texture2D) | details.HeapFlags;
	createInfo.HeapProps = details.Heap ? *details.Heap : ToHeapProps(flags);
	return createInfo;
}

ResourceCreateInfo ResourceCreateHelper::Texture2DArray(uint32_t width, uint32_t height, uint32_t arraySize,
														DXGI_FORMAT format, ResourcePresetFlags flags,
														TextureDetails details)
{
	ResourceCreateInfo createInfo{};
	createInfo.Desc =
		CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, arraySize, !!(flags & ResourcePresetFlags::MipMaps) ? 0 : 1,
									 1, 0, ToResourceFlags(flags) | details.DetailedFlags);
	createInfo.HeapFlags = ToHeapFlags(flags, PresetType::Texture2DArray) | details.HeapFlags;
	createInfo.HeapProps = details.Heap ? *details.Heap : ToHeapProps(flags);
	return createInfo;
}

ResourceCreateInfo ResourceCreateHelper::Texture2DCube(uint32_t width, uint32_t height, DXGI_FORMAT format,
													   ResourcePresetFlags flags,
													   TextureDetails details)
{
	ResourceCreateInfo createInfo{};
	createInfo.Desc =
		CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 6, !!(flags & ResourcePresetFlags::MipMaps) ? 0 : 1, 1, 0,
									 ToResourceFlags(flags) | details.DetailedFlags);
	createInfo.HeapFlags = ToHeapFlags(flags, PresetType::Texture2DCube) | details.HeapFlags;
	createInfo.HeapProps = details.Heap ? *details.Heap : ToHeapProps(flags);
	return createInfo;
}

ResourceCreateInfo ResourceCreateHelper::Texture3D(uint32_t width, uint32_t height, uint32_t depth, DXGI_FORMAT format,
												   ResourcePresetFlags flags,
												   TextureDetails details)
{
	ResourceCreateInfo createInfo{};
	createInfo.Desc =
		CD3DX12_RESOURCE_DESC::Tex3D(format, width, height, depth, !!(flags & ResourcePresetFlags::MipMaps) ? 0 : 1,
									 ToResourceFlags(flags) | details.DetailedFlags);
	createInfo.HeapFlags = ToHeapFlags(flags, PresetType::Texture3D) | details.HeapFlags;
	createInfo.HeapProps = details.Heap ? *details.Heap : ToHeapProps(flags);
	return createInfo;
}

} // namespace rad