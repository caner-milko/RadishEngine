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

using CPUDescriptorDesc =
	std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc, RenderTargetViewDesc,
				 DepthStencilViewDesc, VertexBufferViewDesc, IndexBufferViewDesc>;

using GPUDescriptorDesc = std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc>;

using DescriptorDesc = std::variant<CPUDescriptorDesc, GPUDescriptorDesc>;

struct CPUResourceDescriptor
{
	std::variant<DescriptorAllocation, D3D12_VERTEX_BUFFER_VIEW,
				 D3D12_INDEX_BUFFER_VIEW>
		ViewDesc;
};

using GPUResourceDescriptor = DescriptorAllocation;

using ResourceDescriptor = std::variant<CPUResourceDescriptor, GPUResourceDescriptor>;


struct ResourceCreateInfo
{
	D3D12_RESOURCE_DESC Desc;
	D3D12_RESOURCE_FLAGS Flags;
	D3D12_HEAP_DESC HeapDesc;
	D3D12_HEAP_FLAGS HeapFlags;
	bool operator==(const ResourceCreateInfo& Other) const;
	size_t Hash() const;
};
} // namespace rad

#define RAD_DECLARE_HASH(Type)                                                                                             \
	namespace std                                                                                                      \
	{                                                                                                                  \
	template <> struct hash<Type>                                                                                      \
	{                                                                                                                  \
		size_t operator()(const Type& val) const                                                                       \
		{                                                                                                              \
			return val.Hash();                                                                                         \
		}                                                                                                              \
	};                                                                                                                 \
	}
RAD_DECLARE_HASH(rad::ShaderResourceViewDesc)
RAD_DECLARE_HASH(rad::UnorderedAccessViewDesc)
RAD_DECLARE_HASH(rad::ConstantBufferViewDesc)
RAD_DECLARE_HASH(rad::RenderTargetViewDesc)
RAD_DECLARE_HASH(rad::DepthStencilViewDesc)
RAD_DECLARE_HASH(rad::VertexBufferViewDesc)
RAD_DECLARE_HASH(rad::IndexBufferViewDesc)
RAD_DECLARE_HASH(rad::ResourceCreateInfo)

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
		PoolResourceView AsView();
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
		PoolResourceView AsView();
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
	OwnedResource& GetResource(const ResourceCreateInfo& createInfo, std::string acquireName);
	void FreeResource(OwnedResource& resource);
	ExternalResource& AddExternalResource(ID3D12Resource& resource, std::string name, const ResourceCreateInfo& createInfo,
										  D3D12_RESOURCE_STATES initialState);
	void RemoveExternalResource(ExternalResource& resource);
	ResourceDescriptor& GetDescriptor(const PoolResourceView& resource, const DescriptorDesc& desc);
private:
	Renderer& Renderer;
	Resource& AddResourceInfo(ID3D12Resource& resource, const ResourceCreateInfo& createInfo, D3D12_RESOURCE_STATES initialState);
	std::unordered_map<ResourceCreateInfo, std::deque<OwnedResource>> OwnedResources;
	std::unordered_map<Ref<ID3D12Resource>, Resource> Resources;
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
	bool operator==(const PoolResourceView& Other) const
	{
		return &Info == &Other.Info && &Name == &Other.Name && UnderlyingResource == Other.UnderlyingResource;
	}
	size_t Hash() const
	{
		return HashCombine(Info, Name, UnderlyingResource);
	}
	PoolResourceView(std::variant<Ref<ResourcePool::OwnedResource>, Ref<ResourcePool::ExternalResource>> resInfo)
		: Info(std::visit([](auto&& arg) -> ResourcePool::Resource& { return *arg; }, resInfo)),
		  Name(std::visit([](auto&& arg) -> std::string const& { return arg->GetName(); }, resInfo)),
		  UnderlyingResource(resInfo)
	{
	}
  private:
	Ref<ResourcePool::Resource> Info;
	Ref<const std::string> Name;
	std::variant<Ref<ResourcePool::OwnedResource>, Ref<ResourcePool::ExternalResource>> UnderlyingResource;
	friend struct ResourcePool;
};
} // namespace rad
RAD_DECLARE_HASH(rad::PoolResourceView)
