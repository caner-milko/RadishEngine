#pragma once

#include "RendererCommon.h"

namespace rad
{
struct ShaderResourceViewDesc
{
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc;
	bool operator==(const ShaderResourceViewDesc& Other) const;
	size_t Hash() const;
};

struct UnorderedAccessViewDesc
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC Desc;
	bool operator==(const UnorderedAccessViewDesc& Other) const;
	size_t Hash() const;
};

struct ConstantBufferViewDesc
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC Desc;
	bool operator==(const ConstantBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct RenderTargetViewDesc
{
	D3D12_RENDER_TARGET_VIEW_DESC Desc;
	bool operator==(const RenderTargetViewDesc& Other) const;
	size_t Hash() const;
};

struct DepthStencilViewDesc
{
	D3D12_DEPTH_STENCIL_VIEW_DESC Desc;
	bool operator==(const DepthStencilViewDesc& Other) const;
	size_t Hash() const;
};

struct VertexBufferViewDesc
{
	D3D12_VERTEX_BUFFER_VIEW Desc;
	bool operator==(const VertexBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct IndexBufferViewDesc
{
	D3D12_INDEX_BUFFER_VIEW Desc;
	bool operator==(const IndexBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct CPUDescriptorDesc
{
	std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc, RenderTargetViewDesc,
				 DepthStencilViewDesc, VertexBufferViewDesc,
				 IndexBufferViewDesc>
		ViewDesc;
	bool operator==(const CPUDescriptorDesc& Other) const;
	size_t Hash() const;
};

struct GPUDescriptorDesc
{
	std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc>
		ViewDesc;
	bool operator==(const GPUDescriptorDesc& Other) const;
	size_t Hash() const;
};

using DescriptorDesc = std::variant<CPUDescriptorDesc, GPUDescriptorDesc>;

struct CPUResourceDescriptor
{
	std::variant<D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_VERTEX_BUFFER_VIEW,
				 D3D12_INDEX_BUFFER_VIEW>
		ViewDesc;
};

struct GPUResourceDescriptor
{
	std::variant<D3D12_SHADER_RESOURCE_VIEW_DESC, D3D12_UNORDERED_ACCESS_VIEW_DESC, D3D12_CONSTANT_BUFFER_VIEW_DESC>
		ViewDesc;
};

using ResourceDescriptor = std::variant<CPUResourceDescriptor, GPUResourceDescriptor>;

struct TextureCreateInfo
{
	D3D12_RESOURCE_DESC Desc;
	bool operator==(const TextureCreateInfo& Other) const;
	size_t Hash() const;
};

struct BufferCreateInfo
{
	D3D12_RESOURCE_DESC Desc;
	bool operator==(const BufferCreateInfo& Other) const;
	size_t Hash() const;
};

template <typename DXType, typename RGCreateInfo> struct ResourceTemplate;

using Texture = ResourceTemplate<DXTexture, TextureCreateInfo>;
using Buffer = ResourceTemplate<DXBuffer, BufferCreateInfo>;

using ResourceCreateInfo = std::variant<TextureCreateInfo, BufferCreateInfo>;
} // namespace rad

namespace std
{
#define DECLARE_HASH(Type)                                                                                             \
	template <> struct hash<Type>                                                                                      \
	{                                                                                                                  \
		size_t operator()(const Type& val) const                                                                       \
		{                                                                                                              \
			return val.Hash();                                                                                         \
		}                                                                                                              \
	};
DECLARE_HASH(rad::ShaderResourceViewDesc)
DECLARE_HASH(rad::UnorderedAccessViewDesc)
DECLARE_HASH(rad::ConstantBufferViewDesc)
DECLARE_HASH(rad::RenderTargetViewDesc)
DECLARE_HASH(rad::DepthStencilViewDesc)
DECLARE_HASH(rad::VertexBufferViewDesc)
DECLARE_HASH(rad::IndexBufferViewDesc)
DECLARE_HASH(rad::CPUDescriptorDesc)
DECLARE_HASH(rad::GPUDescriptorDesc)
DECLARE_HASH(rad::TextureCreateInfo)
DECLARE_HASH(rad::BufferCreateInfo)
#undef DECLARE_HASH
} // namespace std

namespace rad
{
struct Resource
{
	ResourceCreateInfo CreateInfo;
	std::variant<DXTexture, DXBuffer> Resource;
	std::unordered_map<DescriptorDesc, std::deque<ResourceDescriptor>> Descriptors;
};

struct ResourcePool
{
	std::unordered_map<ResourceCreateInfo, std::unordered_set<std::unique_ptr<Resource>>> Resources;

	Resource& GetResource(const ResourceCreateInfo& CreateInfo);
	ResourceDescriptor& GetDescriptor(const ResourceCreateInfo& CreateInfo, const DescriptorDesc& Desc);
};
} // namespace rad