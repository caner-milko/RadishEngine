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

Resource& ResourcePool::GetResource(const ResourceCreateInfo& CreateInfo)
{
	// TODO: insert return statement here
}
}