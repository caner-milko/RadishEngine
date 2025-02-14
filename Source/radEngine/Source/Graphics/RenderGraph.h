#pragma once
#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{

template <typename T> struct RGFuture
{
	operator T&()
	{
		return *Future;
	}
	bool HasValue()
	{
		return Future.has_value();
	}
	T& Get()
	{
		return *Future;
	}

  protected:
	RGFuture() {}
	std::optional<T> Future = std::nullopt;
	void ResourceDecided(T&& resource)
	{
		assert(!Future);
		Future = std::move(resource);
	}
	friend struct RGResourceManager;
};

struct RGResourceUsage
{
	D3D12_RESOURCE_STATES State;
	DescriptorDesc DescriptorDesc;

	bool IsCompatibleWith(const RGResourceUsage& other) const
	{
		return true;
	}
};

struct RGGraphResource : RGFuture<PoolResourceView>
{
	RGGraphResource(ResourceCreateInfo createInfo) : CreateInfo(createInfo) {}
	ResourceCreateInfo CreateInfo;

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
	PoolResourceView& GetResource()
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(this))
		{
			return (*tempResource)->Get();
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

	operator PoolResourceView&()
	{
		return GetResource();
	}
	PoolResourceView* operator->()
	{
		return &GetResource();
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

using RGResourceDescriptor = RGFuture<ResourceDescriptor>;

struct RGResourceViewBase : RGResourceRef
{
	RGResourceViewBase(RGResourceRef resourceRef, RGResourceDescriptor& descriptor)
		: RGResourceRef(resourceRef), Descriptor(descriptor)
	{
	}
	Ref<RGResourceDescriptor> Descriptor;
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
					 RGResourceDescriptor& descriptor)
		: Name(std::move(name)), OwnerPass(ownerPass), Source(source), Usage(std::move(usage)), Descriptor(descriptor)
	{
	}

	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	Ref<RGBOutputResource> Source;
	RGResourceUsage Usage;
	Ref<RGResourceDescriptor> Descriptor;

	operator RGResourceViewBase()
	{
		return RGResourceViewBase{Source->ResourceRef, Descriptor};
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
	std::unordered_map<RGResourceRef, std::unordered_map<DescriptorDesc, RGResourceDescriptor>>
		CreatedGraphResourcesMap;
	RGResourceDescriptor& GetDescriptor(RGResourceRef const& resource, DescriptorDesc const& desc)
	{
		return CreatedGraphResourcesMap[resource].try_emplace(desc, RGResourceDescriptor{}).first->second;
	}
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
	RGBOutputResource& AddExternalResource(PoolResourceView& externalResource);
	void Build(Renderer& renderer, CommandContext& cmd);

private:
	RGBOutputResource& InitializeResourceProvider(std::string name, RGResourceRef resourceRef);
};

};