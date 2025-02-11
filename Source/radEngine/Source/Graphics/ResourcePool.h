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
	Ref<ID3D12Resource> Resource;
	D3D12_RESOURCE_STATES State;
	std::unordered_map<DescriptorDesc, ResourceDescriptor> Descriptors;
};


struct ResourcePool
{
	struct OwnedResource
	{
		operator rad::Resource&()
		{
			return *Info;
		}
		operator const rad::Resource&() const
		{
			return *Info;
		}
		rad::Resource* operator->()
		{
			return &Info;
		}
		const rad::Resource* operator->() const
		{
			return &Info;
		}
		std::string const& GetName() const
		{
			assert(AcquiredName.has_value());
			return *AcquiredName;
		}
	  private:
		friend struct ResourcePool;
		OwnedResource(ComPtr<ID3D12Resource> resource, rad::Resource& resInfo) : Resource(resource), Info(resInfo) {}
		ComPtr<ID3D12Resource> Resource;
		Ref<rad::Resource> Info;
		std::optional<std::string> AcquiredName;
	};
	struct ExternalResource
	{
		operator rad::Resource&()
		{
			return *Info;
		}
		operator const rad::Resource&() const
		{
			return *Info;
		}
		rad::Resource* operator->()
		{
			return &Info;
		}
		const rad::Resource* operator->() const
		{
			return &Info;
		}
		std::string const& GetName() const
		{
			return Name;
		}
	  private:
		ExternalResource(ID3D12Resource& resource, std::string name, rad::Resource& resInfo) : Resource(resource), Name(std::move(name)), Info(resInfo) {}
		Ref<ID3D12Resource> Resource;
		std::string Name;
		Ref<rad::Resource> Info;
		friend struct ResourcePool;
	};
	ResourcePool(Renderer& renderer);
	Renderer& Renderer;
	OwnedResource& GetResource(const ResourceCreateInfo& createInfo, std::string acquireName);
	void FreeResource(OwnedResource& resource);
	ExternalResource& AddExternalResource(ID3D12Resource& resource, std::string name, const ResourceCreateInfo& createInfo,
										  D3D12_RESOURCE_STATES initialState);
	ResourceDescriptor& GetDescriptor(ID3D12Resource& resourceRef, const DescriptorDesc& desc);
private:
	Resource& AddResourceInfo(ID3D12Resource& resource, const ResourceCreateInfo& createInfo, D3D12_RESOURCE_STATES initialState);
	std::unordered_map<ResourceCreateInfo, std::deque<OwnedResource>> OwnedResources;
	std::unordered_map<Ref<ID3D12Resource>, Resource> Resources;
	std::unordered_map<Ref<ID3D12Resource>, ExternalResource> ExternalResources;
	std::unordered_map<ResourceCreateInfo, std::deque<Ref<OwnedResource>>> FreeResources;
};
} // namespace rad