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
	RGGraphResource(std::string name, RGResourceCreateInfo createInfo) : Name(std::move(name)), CreateInfo(createInfo)
	{
	}
	std::string Name;
	RGResourceCreateInfo CreateInfo;

	friend struct RGResourceManager;
	friend struct RGResourceViewBase;

  private:
	DXResource* AssociatedResource;
};

struct RGExternalResource
{
	RGExternalResource(std::string name, DXResource& resource, RGResourceCreateInfo createInfo, RGResourceUsage initialUsage)
		: Name(std::move(name)), Resource(resource), CreateInfo(std::move(createInfo)), InitialUsage(std::move(initialUsage))
	{
	}
	std::string Name;
	Ref<DXResource> Resource;
	RGResourceCreateInfo CreateInfo;
	RGResourceUsage InitialUsage;
};

using RGResourceRef = std::variant<Ref<RGGraphResource>, Ref<RGExternalResource>>;

struct RGResourceViewBase
{
	RGResourceRef Resource;
	Ref<RGResourceDescriptor> Descriptor;

	RGResourceCreateInfo& GetCreateInfo()
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(&Resource))
			return (*tempResource)->CreateInfo;
		else
			return (*std::get_if<Ref<RGExternalResource>>(&Resource))->CreateInfo;
	}

	/// Returns the underlying DXResource
	DXResource& GetResource() 
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(&Resource))
		{
			assert((*tempResource)->AssociatedResource);
			return *(*tempResource)->AssociatedResource;
		}
		else
			return (*std::get_if<Ref<RGExternalResource>>(&Resource))->Resource;
	}
	operator DXResource&()
	{
		return GetResource();
	}
};

template<typename T>
requires std::is_base_of_v<DXResource, T>
struct RGResourceViewT : RGResourceViewBase
{
	T& GetResource()
	{
		return static_cast<T&>(RGResourceView::GetResource());
	}
	operator T&()
	{
		return GetResource();
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
					 RGResourceDescriptor& descriptor)
		: Name(std::move(name)), OwnerPass(ownerPass), Source(source), Usage(std::move(usage)), Descriptor(descriptor)
	{
	}

	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	Ref<RGBOutputResource> Source;
	RGResourceUsage Usage;
	Ref<RGResourceDescriptor> Descriptor;

	template <typename T>
		requires std::is_base_of_v<DXResource, T>
	operator RGResourceViewT<T>()
	{
		return static_cast<RGResourceViewT<T>>(ResourceView);
	}
};

struct RenderPassBuilder
{
	RenderPassBuilder(std::string name, RenderGraphBuilder& rgBuilder) : Name(std::move(name)), RGBuilder(&rgBuilder) {}
	
	std::string Name;
	std::deque<RGBInputResource> Inputs;
	std::deque<RGBOutputResource> Outputs;
	std::function<void(CommandContext&)> Execute;
	Ref<RenderGraphBuilder> RGBuilder;
#define RAD_RENDER_PASS_RESOURCE_TEX(Name, Variable) Name = static_cast<RGResourceViewT<DXTexture>>(Variable))
#define RAD_RENDER_PASS_RESOURCE_BUF(Name, Variable) Name = static_cast<RGResourceViewT<DXBuffer>>(Variable))

	RGBInputResource& AddInput(std::string name, RGBOutputResource& resource, RGResourceUsage usage);
	std::pair<RGBInputResource&, RGBOutputResource&> AddInOutResource(std::string name, RGBOutputResource& resource, RGResourceUsage usage);
};

struct RGResourceManager
{
	struct RGDecidedResource
	{
		std::deque<std::pair<RGDescriptorDesc, RGResourceDescriptor>> Descriptors;
		DXResource* ResourceToBeDecided;
	};
	std::deque<RGExternalResource> ExternalResources;
	std::deque<RGGraphResource> GraphResources;
	std::unordered_map<RGResourceRef, RGDecidedResource> ResourceMap;

	DXResource* CreateResource(const RGResourceCreateInfo& createInfo, RGResourceUsage initialUsage);
	RGResourceDescriptor& GetDescriptor(RGResourceRef resource, RGDescriptorDesc descriptorDesc);
};

struct RenderGraphBuilder
{
	RGResourceManager ResourceManager;
	std::deque<RenderPassBuilder> Passes;

	RenderPassBuilder& AddPass(std::string name)
	{
		return Passes.emplace_back(std::move(name));
	}
	RGBOutputResource& AddGraphResource(std::string name, RGResourceCreateInfo createInfo);
	RGBOutputResource& AddExternalResource(std::string name, DXResource& resource, RGResourceCreateInfo createInfo, RGResourceUsage initialUsage);
	void Build(Renderer& renderer, CommandContext& cmd);

private:
	RGBOutputResource& InitializeResourceProvider(std::string name, RGResourceRef resourceRef);
};

};