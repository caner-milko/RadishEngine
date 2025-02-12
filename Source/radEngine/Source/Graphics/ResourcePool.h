#pragma once

#include "RendererCommon.h"

namespace rad
{
struct ShaderResourceViewDesc : D3D12_SHADER_RESOURCE_VIEW_DESC
{
	bool operator==(const ShaderResourceViewDesc& Other) const;
	size_t Hash() const;
};

struct UnorderedAccessViewDesc : D3D12_UNORDERED_ACCESS_VIEW_DESC
{
	bool operator==(const UnorderedAccessViewDesc& Other) const;
	size_t Hash() const;
};

struct ConstantBufferViewDesc : D3D12_CONSTANT_BUFFER_VIEW_DESC
{
	bool operator==(const ConstantBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct RenderTargetViewDesc : D3D12_RENDER_TARGET_VIEW_DESC
{
	bool operator==(const RenderTargetViewDesc& Other) const;
	size_t Hash() const;
};

struct DepthStencilViewDesc : D3D12_DEPTH_STENCIL_VIEW_DESC
{
	bool operator==(const DepthStencilViewDesc& Other) const;
	size_t Hash() const;
};

struct VertexBufferViewDesc : D3D12_VERTEX_BUFFER_VIEW
{
	bool operator==(const VertexBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct IndexBufferViewDesc : D3D12_INDEX_BUFFER_VIEW
{
	bool operator==(const IndexBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct CPUDescriptorDesc
	: std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc, RenderTargetViewDesc,
				   DepthStencilViewDesc, VertexBufferViewDesc, IndexBufferViewDesc>
{
	bool operator==(const CPUDescriptorDesc& Other) const;
	size_t Hash() const;
};

struct GPUDescriptorDesc : std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc>
{
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

struct TextureCreateInfo : D3D12_RESOURCE_DESC
{
	bool operator==(const TextureCreateInfo& Other) const;
	size_t Hash() const;
};

struct BufferCreateInfo : D3D12_RESOURCE_DESC
{
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
struct PoolResourceView;
struct ResourcePool
{
	struct Resource
	{
		ResourceCreateInfo CreateInfo;
		Ref<ID3D12Resource> DXRes;
		D3D12_RESOURCE_STATES State;
		std::unordered_map<DescriptorDesc, ResourceDescriptor> Descriptors;

		Resource(Resource&&) = default;
		Resource& operator=(Resource&&) = default;

	  private:
		friend struct ResourcePool;
		Resource(ResourceCreateInfo createInfo, ID3D12Resource& resource, D3D12_RESOURCE_STATES initialState)
			: CreateInfo(createInfo), DXRes(resource), State(initialState)
		{
		}
		Resource(const Resource&) = delete;
		Resource& operator=(const Resource&) = delete;
	};
	struct OwnedResource
	{
		operator Resource&()
		{
			return *Info;
		}
		operator const Resource&() const
		{
			return *Info;
		}
		Resource* operator->()
		{
			return &Info;
		}
		const Resource* operator->() const
		{
			return &Info;
		}
		std::string const& GetName() const
		{
			assert(AcquiredName.has_value());
			return *AcquiredName;
		}
		OwnedResource(OwnedResource&&) = default;
		OwnedResource& operator=(OwnedResource&&) = default;
	  private:
		friend struct ResourcePool;
		OwnedResource(ComPtr<ID3D12Resource> resource, Resource& resInfo) : DXRes(resource), Info(resInfo) {}
		OwnedResource(const OwnedResource&) = delete;
		OwnedResource& operator=(const OwnedResource&) = delete;
		ComPtr<ID3D12Resource> DXRes;
		Ref<Resource> Info;
		std::optional<std::string> AcquiredName;
	};
	struct ExternalResource
	{
		operator Resource&()
		{
			return *Info;
		}
		operator const Resource&() const
		{
			return *Info;
		}
		Resource* operator->()
		{
			return &Info;
		}
		const Resource* operator->() const
		{
			return &Info;
		}
		std::string const& GetName() const
		{
			return Name;
		}
		ExternalResource(ExternalResource&&) = default;
		ExternalResource& operator=(ExternalResource&&) = default;
	  private:
		ExternalResource(ID3D12Resource& resource, std::string name, Resource& resInfo) : DXRes(resource), Name(std::move(name)), Info(resInfo) {}
		ExternalResource(const ExternalResource&) = delete;
		ExternalResource& operator=(const ExternalResource&) = delete;
		Ref<ID3D12Resource> DXRes;
		std::string Name;
		Ref<Resource> Info;
		friend struct ResourcePool;
	};
	ResourcePool(Renderer& renderer);
	PoolResourceView GetResource(const ResourceCreateInfo& createInfo, std::string acquireName);
	void FreeResource(OwnedResource& resource);
	PoolResourceView AddExternalResource(ID3D12Resource& resource, std::string name, const ResourceCreateInfo& createInfo,
										  D3D12_RESOURCE_STATES initialState);
	ResourceDescriptor& GetDescriptor(PoolResourceView& resource, const DescriptorDesc& desc);
private:
	Renderer& Renderer;
	Resource& AddResourceInfo(PoolResourceView resourceView, const ResourceCreateInfo& createInfo, D3D12_RESOURCE_STATES initialState);
	std::unordered_map<ResourceCreateInfo, std::deque<OwnedResource>> OwnedResources;
	std::unordered_map<std::variant<Ref<OwnedResource>, Ref<ExternalResource>>, Resource> Resources;
	std::unordered_map<Ref<ID3D12Resource>, ExternalResource> ExternalResources;
	std::unordered_map<ResourceCreateInfo, std::deque<Ref<OwnedResource>>> FreeResources;
};
struct PoolResourceView
{
	operator ResourcePool::Resource&()
	{
		return *Info;
	}
	operator const ResourcePool::Resource&() const
	{
		return *Info;
	}
	ResourcePool::Resource* operator->()
	{
		return &Info;
	}
	const ResourcePool::Resource* operator->() const
	{
		return &Info;
	}
	std::string const& GetName() const
	{
		return Name;
	}

  private:
	PoolResourceView(std::variant<Ref<ResourcePool::OwnedResource>, Ref<ResourcePool::ExternalResource>> resInfo)
		: Info(std::visit([](auto&& arg) -> ResourcePool::Resource& { return *arg; }, resInfo)),
		  Name(std::visit([](auto&& arg) -> std::string const& { return arg->GetName(); }, resInfo)),
		  UnderlyingResource(resInfo)
	{
	}
	Ref<ResourcePool::Resource> Info;
	Ref<const std::string> Name;
	std::variant<Ref<ResourcePool::OwnedResource>, Ref<ResourcePool::ExternalResource>> UnderlyingResource;
	friend struct ResourcePool;
};
} // namespace rad