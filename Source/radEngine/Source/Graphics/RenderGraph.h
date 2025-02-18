#pragma once
#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{

template <typename T, bool Ref> struct RGFuture
{
	operator T&()
	{
		return *Future;
	}
	bool HasValue()
	{
		return !!Future;
	}
	T& Get()
	{
		return *Future;
	}

  protected:
	RGFuture() {}
	std::conditional_t<Ref, T*, std::optional<T>> Future = {};
	void ResourceDecided(std::conditional_t<Ref, T&, T&&> resource)
	{
		assert(!Future);
		if constexpr (Ref)
			Future = &resource;
		else
			Future = std::move(resource);
	}
	friend struct RGResourceManager;
};

struct RGResourceUsage
{
	D3D12_RESOURCE_STATES State;
	std::optional<DescriptorDesc> DescriptorDesc = std::nullopt;

	bool IsCompatibleWith(const RGResourceUsage& other) const
	{
		return true;
	}
};

struct RGGraphResource : RGFuture<ResourcePool::OwnedResource, true>
{
	RGGraphResource(ResourceCreateInfo createInfo, std::string name) : CreateInfo(createInfo), Name(std::move(name)) {}
	ResourceCreateInfo CreateInfo;
	std::string Name;

	friend struct RGResourceManager;
	friend struct RGResourceRef;
};

using RGExternalResourceRef = PoolResourceView;

struct RGResourceRef : std::variant<Ref<RGGraphResource>, RGExternalResourceRef>
{
	using std::variant<Ref<RGGraphResource>, RGExternalResourceRef>::variant;
	using std::variant<Ref<RGGraphResource>, RGExternalResourceRef>::operator=;
	RGResourceRef(RGGraphResource& resource) : std::variant<Ref<RGGraphResource>, RGExternalResourceRef>(Ref(resource))
	{
	}
	PoolResourceView GetResource() const
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(this))
		{
			return (*tempResource)->Get().AsView();
		}
		else
			return (*std::get_if<RGExternalResourceRef>(this));
	}


	ResourceCreateInfo& GetCreateInfo()
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(this))
		{
			if ((*tempResource)->HasValue())
				return (*tempResource)->Get()->CreateInfo;
			// Is this really used?
			assert(false);
			return (*tempResource)->CreateInfo;
		}
		else
			return (*std::get_if<RGExternalResourceRef>(this))->CreateInfo;
	}

	operator PoolResourceView()
	{
		return GetResource();
	}
	PoolResourceView operator->()
	{
		return GetResource();
	}

	RGGraphResource* AsGraphResource()
	{
		if (auto* temp = std::get_if<Ref<RGGraphResource>>(this))
			return &*temp;
		return nullptr;
	}
	RGExternalResourceRef* AsExternalResource()
	{
		return std::get_if<RGExternalResourceRef>(this);
	}
};
} // namespace rad
namespace std
{
template <> struct hash<rad::RGResourceRef>
{
	size_t operator()(const rad::RGResourceRef& resource) const
	{
		return std::hash<std::variant<rad::Ref<rad::RGGraphResource>, rad::RGExternalResourceRef>>{}(resource);
	}
};
} // namespace std

namespace rad
{

using RGResourceDescriptor = RGFuture<ResourceDescriptor, true>;

struct RGResourceViewBase : RGResourceRef
{
	RGResourceViewBase(RGResourceRef resourceRef, OptionalRef<RGResourceDescriptor> descriptor)
		: RGResourceRef(resourceRef), DescriptorRef(descriptor)
	{
	}

	template<typename T>
		requires std::is_same_v<T, RenderTargetViewDesc> || std::is_same_v<T, DepthStencilViewDesc> ||
				 std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
				 std::is_same_v<T, ConstantBufferViewDesc> || std::is_same_v<T, VertexBufferViewDesc> ||
				 std::is_same_v<T, IndexBufferViewDesc>
	auto& AsCPUDescriptor()
	{
		return Descriptor().AsCPUDescriptor<T>();
	}

	template <typename T>
		requires std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
				 std::is_same_v<T, ConstantBufferViewDesc>
	auto& AsGPUDescriptor()
	{
		return std::get<GPUResourceDescriptor>(*this);
	}

	ResourceDescriptor& Descriptor()
	{
		return (*DescriptorRef).Get();
	}
	OptionalRef<RGResourceDescriptor> DescriptorRef;
};

struct RenderPassBuilder;
struct RGBInputResource;
struct RenderGraphBuilder;

struct RGBOutputResource
{
	RGBOutputResource(std::string name, RenderPassBuilder& ownerPass, RGResourceRef resourceRef)
		: Name(std::move(name)), OwnerPass(ownerPass), ResourceRef(resourceRef)
	{
	}

	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	RGResourceRef ResourceRef;
	std::vector<Ref<RGBInputResource>> ConnectedInputs;
	void AddInput(RGBInputResource& input)
	{
		ConnectedInputs.push_back(input);
	}
};

struct RGBInputResource
{
	RGBInputResource(std::string name, RenderPassBuilder& ownerPass, RGBOutputResource& source, RGResourceUsage usage,
					 OptionalRef<RGResourceDescriptor> descriptor)
		: Name(std::move(name)), OwnerPass(ownerPass), Source(source), Usage(std::move(usage)), Descriptor(descriptor)
	{
	}


	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	Ref<RGBOutputResource> Source;
	RGResourceUsage Usage;
	OptionalRef<RGResourceDescriptor> Descriptor;

	RGResourceViewBase GetResourceView()
	{
		return RGResourceViewBase{Source->ResourceRef, Descriptor};
	}
	operator RGResourceViewBase()
	{
		return GetResourceView();
	}
	ResourcePool::Resource* operator->()
	{
		return GetResourceView().GetResource().operator->();
	}
};

struct RenderPassBuilder
{
	RenderPassBuilder(std::string name, RenderGraphBuilder& rgBuilder) : Name(std::move(name)), RGBuilder(rgBuilder) {}
	
	std::string Name;
	std::deque<RGBInputResource> Inputs;
	std::deque<RGBOutputResource> Outputs;
	std::function<void(CommandContext&)> Execute;
	Ref<RenderGraphBuilder> RGBuilder;

	RGBInputResource& AddInput(std::string name, RGBOutputResource& resource, RGResourceUsage usage);
	std::pair<RGBInputResource&, RGBOutputResource&> AddInOutResource(std::string name, RGBOutputResource& resource, RGResourceUsage usage);
};

struct RGResourceManager
{
	std::deque<PoolResourceView> ExternalResources;
	std::deque<RGGraphResource> GraphResources;
	struct ResourceInfo
	{
		std::unordered_map<DescriptorDesc, RGResourceDescriptor> Descriptors;
		D3D12_RESOURCE_STATES LastState;
	};
	std::unordered_map<RGResourceRef, ResourceInfo> CreatedGraphResourcesMap;
	PoolResourceView& AddExternalResource(PoolResourceView resource);
	RGGraphResource& AddGraphResource(ResourceCreateInfo createInfo, std::string name);
	RGResourceDescriptor& GetDescriptor(RGResourceRef const& resource, DescriptorDesc const& desc);
	D3D12_RESOURCE_STATES& GetLastState(RGResourceRef const& resource);
	void CreateResourcesAndDescriptors(Renderer& renderer);
	void FreeResources(Renderer& renderer);
};

struct RenderGraphBuilder
{
	RGResourceManager ResourceManager;
	std::deque<RenderPassBuilder> Passes;

	RenderPassBuilder& AddPass(std::string name)
	{
		return Passes.emplace_back(std::move(name), *this);
	}
	RGBOutputResource& AddGraphResource(std::string name, ResourceCreateInfo createInfo);
	RGBOutputResource& AddExternalResource(PoolResourceView externalResource);

	void BuildAndExecute(Renderer& renderer, CommandContext& cmd);

private:
	RGBOutputResource& InitializeResourceProvider(std::string name, RGResourceRef resourceRef);
};

};