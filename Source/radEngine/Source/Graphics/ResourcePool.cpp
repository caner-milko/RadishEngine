#include "ResourcePool.h"

namespace rad
{
template <typename T, auto... Desc> bool CompareDesc(const T& Desc1, const T& Desc2)
{
	return ((Desc1.*Desc == Desc2.*Desc) && ...);
}

bool ShaderResourceViewDesc::operator==(const ShaderResourceViewDesc& Other) const
{
	if (Other.Desc.Format != Desc.Format || Other.Desc.ViewDimension != Desc.ViewDimension ||
		Other.Desc.Shader4ComponentMapping != Desc.Shader4ComponentMapping)
		return false;

	switch (Desc.ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_SRV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		auto& otherDesc = Other.Desc.ViewName;                                                                         \
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
	}
}
size_t ShaderResourceViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.Format, Desc.ViewDimension, Desc.Shader4ComponentMapping);
	switch (Desc.ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_SRV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		HashCombine(hash, __VA_ARGS__);                                                                          \
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
	}
	return hash;
}

bool UnorderedAccessViewDesc::operator==(const UnorderedAccessViewDesc& Other) const
{
	if (Other.Desc.Format != Desc.Format || Other.Desc.ViewDimension != Desc.ViewDimension)
		return false;
	switch (Desc.ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_UAV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		auto& otherDesc = Other.Desc.ViewName;                                                                         \
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
	}
}
size_t UnorderedAccessViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.Format, Desc.ViewDimension);
	switch (Desc.ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_UAV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		HashCombine(hash, desc, __VA_ARGS__);                                                                          \
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
	}
	return hash;
}

bool ConstantBufferViewDesc::operator==(const ConstantBufferViewDesc& Other) const
{
	return Desc.BufferLocation == Other.Desc.BufferLocation && Desc.SizeInBytes == Other.Desc.SizeInBytes;
}
size_t ConstantBufferViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.BufferLocation, Desc.SizeInBytes);
	return hash;
}

bool RenderTargetViewDesc::operator==(const RenderTargetViewDesc& Other) const
{
	if (Other.Desc.Format != Desc.Format || Other.Desc.ViewDimension != Desc.ViewDimension)
		return false;
	switch (Desc.ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_RTV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		auto& otherDesc = Other.Desc.ViewName;                                                                         \
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
	}
}
size_t RenderTargetViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.Format, Desc.ViewDimension);
	switch (Desc.ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)     \
	case D3D12_RTV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		HashCombine(hash, desc, __VA_ARGS__);                                                                          \
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
	}
	return hash;
}

bool DepthStencilViewDesc::operator==(const DepthStencilViewDesc& Other) const
{
	if (Other.Desc.Format != Desc.Format || Other.Desc.ViewDimension != Desc.ViewDimension)
		return false;
	switch (Desc.ViewDimension)
	{
#define COMPARE_VIEW(ViewType, ViewName, ...)                                                                          \
	case D3D12_DSV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		auto& otherDesc = Other.Desc.ViewName;                                                                         \
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
	}
}
size_t DepthStencilViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.Format, Desc.ViewDimension);
	switch (Desc.ViewDimension)
	{
#define HASH_VIEW(ViewType, ViewName, ...)                                                                             \
	case D3D12_DSV_DIMENSION_##ViewType:                                                                               \
	{                                                                                                                  \
		auto& desc = Desc.ViewName;                                                                                    \
		HashCombine(hash, desc, __VA_ARGS__);                                                                          \
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
	}
	return hash;
}

bool VertexBufferViewDesc::operator==(const VertexBufferViewDesc& Other) const
{
	return Desc.BufferLocation == Other.Desc.BufferLocation && Desc.SizeInBytes == Other.Desc.SizeInBytes &&
		   Desc.StrideInBytes == Other.Desc.StrideInBytes;
}
size_t VertexBufferViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.BufferLocation, Desc.SizeInBytes, Desc.StrideInBytes);
	return hash;
}

bool IndexBufferViewDesc::operator==(const IndexBufferViewDesc& Other) const
{
	return Desc.BufferLocation == Other.Desc.BufferLocation && Desc.SizeInBytes == Other.Desc.SizeInBytes &&
		   Desc.Format == Other.Desc.Format;
}
size_t IndexBufferViewDesc::Hash() const
{
	size_t hash = 0;
	HashCombine(hash, Desc.BufferLocation, Desc.SizeInBytes, Desc.Format);
	return hash;
}

ResourcePool::ResourcePool(rad::Renderer& renderer) : Renderer(renderer) {}

const ResourcePool::OwnedResource& ResourcePool::GetResource(const ResourceCreateInfo& createInfo)
{
	// Create ID3D12Resource from CreateInfo
	ComPtr<ID3D12Resource> resource;

	auto& resInfo = AddResourceInfo(*resource.Get(), createInfo, D3D12_RESOURCE_STATE_COMMON);
	auto& ownedResource = OwnedResources[createInfo].emplace_back(resource, resInfo);
	FreeResources[createInfo].emplace_back(ownedResource);
	return ownedResource;
}

void ResourcePool::FreeResource(const OwnedResource& resource) 
{
	FreeResources[resource->CreateInfo].emplace_back(resource);
}

Resource& ResourcePool::AddResourceInfo(ID3D12Resource& externalResource, const ResourceCreateInfo& createInfo,
											D3D12_RESOURCE_STATES initialState)
{
	return Resources.insert_or_assign(externalResource, Resource{createInfo, externalResource, initialState}).first->second;
}
ResourceDescriptor& ResourcePool::GetDescriptor(ID3D12Resource& createInfo, const DescriptorDesc& desc) 
{
	assert(Resources.contains(createInfo) && "Resource not found in pool");
	auto& resource = Resources.at(createInfo);
	auto& descriptors = resource.Descriptors;
	if (auto it = descriptors.find(desc); it != descriptors.end())
		return it->second;

	// Create descriptor
}
}