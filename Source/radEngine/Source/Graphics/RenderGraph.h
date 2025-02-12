#pragma once
#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{

struct RGResourceUsage
{
	D3D12_RESOURCE_STATES State;
	DescriptorDesc DescriptorDesc;

	bool IsCompatibleWith(const RGResourceUsage& other) const
	{
		return true;
	}
};

struct RGGraphResource
{
	RGGraphResource(ResourceCreateInfo createInfo) : CreateInfo(createInfo)
	{
	}
	ResourceCreateInfo CreateInfo;

	friend struct RGResourceManager;
	friend struct RGResourceViewBase;

  private:
	std::optional<PoolResourceView> AssociatedResource;
};

using RGExternalResourceRef = PoolResourceView;

using RGResourceRef = std::variant<Ref<RGGraphResource>, RGExternalResourceRef>;

struct RGResourceViewBase
{
	RGResourceRef Resource;
	Ref<ResourceDescriptor> Descriptor;

	ResourceCreateInfo& GetCreateInfo()
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(&Resource))
		{
			if (auto& res = (*tempResource)->AssociatedResource)
				return (*res)->CreateInfo;
			// Is this really used?
			assert(false);
			return (*tempResource)->CreateInfo;
		}
		else
			return (*std::get_if<RGExternalResourceRef>(&Resource))->CreateInfo;
	}

	/// Returns the underlying DXResource
	PoolResourceView& GetResource() 
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(&Resource))
		{
			assert((*tempResource)->AssociatedResource);
			return *(*tempResource)->AssociatedResource;
		}
		else
			return (*std::get_if<RGExternalResourceRef>(&Resource));
	}
	operator PoolResourceView&()
	{
		return GetResource();
	}
	PoolResourceView* operator->()
	{
		return &GetResource();
	}
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
					 ResourceDescriptor& descriptor)
		: Name(std::move(name)), OwnerPass(ownerPass), Source(source), Usage(std::move(usage)), Descriptor(descriptor)
	{
	}

	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	Ref<RGBOutputResource> Source;
	RGResourceUsage Usage;
	Ref<ResourceDescriptor> Descriptor;

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
	std::unordered_map<RGResourceRef, PoolResourceView> CreatedGraphResourcesMap;
};

struct RenderGraphBuilder
{
	RGResourceManager ResourceManager;
	std::deque<RenderPassBuilder> Passes;

	RenderPassBuilder& AddPass(std::string name)
	{
		return Passes.emplace_back(std::move(name));
	}
	RGBOutputResource& AddGraphResource(std::string name, ResourceCreateInfo createInfo);
	RGBOutputResource& AddExternalResource(PoolResourceView& externalResource);
	void Build(Renderer& renderer, CommandContext& cmd);

private:
	RGBOutputResource& InitializeResourceProvider(std::string name, RGResourceRef resourceRef);
};

};