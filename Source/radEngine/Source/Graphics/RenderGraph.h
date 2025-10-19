#pragma once
#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{

template <typename T, bool Ref>
struct RGFuture
{
	operator T&() const { return *Future; }
	bool HasValue() const { return !!Future; }
	T& Get() const { return *Future; }

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

	const ResourceCreateInfo& GetCreateInfo() const
	{
		if (auto tempResource = std::get_if<Ref<RGGraphResource>>(this))
		{
			if ((*tempResource)->HasValue())
				return (*tempResource)->Get()->CreateInfo;
			return (*tempResource)->CreateInfo;
		}
		else
			return (*std::get_if<RGExternalResourceRef>(this))->CreateInfo;
	}

	RGGraphResource* AsGraphResource()
	{
		if (auto* temp = std::get_if<Ref<RGGraphResource>>(this))
			return &*temp;
		return nullptr;
	}
	RGExternalResourceRef* AsExternalResource() { return std::get_if<RGExternalResourceRef>(this); }
};
} // namespace rad
namespace std
{
template <>
struct hash<rad::RGResourceRef>
{
	size_t operator()(const rad::RGResourceRef& resource) const
	{
		return std::hash<std::variant<rad::Ref<rad::RGGraphResource>, rad::RGExternalResourceRef>>{}(resource);
	}
};
template <>
struct hash<std::pair<rad::RGResourceRef, rad::DescriptorDesc>>
{
	size_t operator()(const std::pair<rad::RGResourceRef, rad::DescriptorDesc>& pair) const
	{
		return rad::HashCombine(std::hash<rad::RGResourceRef>{}(pair.first),
								std::hash<rad::DescriptorDesc>{}(pair.second));
	}
};
} // namespace std

namespace rad
{

struct RGResourceUsage
{
	template <typename... Descs>
		requires(std::is_same_v<Descs, DescriptorDesc> && ...)
	RGResourceUsage(D3D12_RESOURCE_STATES state, Descs... descs) : State(state), DescriptorDescs{descs...}
	{
	}
	D3D12_RESOURCE_STATES State;
	std::vector<DescriptorDesc> DescriptorDescs;

	bool IsCompatibleWith(const RGResourceUsage& other) const { return true; }

	static RGResourceUsage PixelShaderResourceView(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
								   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
							   DescriptorCreateHelper::ShaderResourceView(resource.GetCreateInfo())};
	}
	static RGResourceUsage NonPixelShaderResourceView(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
							   DescriptorCreateHelper::ShaderResourceView(resource.GetCreateInfo())};
	}
	static RGResourceUsage UnorderedAccessView(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
							   DescriptorCreateHelper::UnorderedAccessView(resource.GetCreateInfo())};
	}
	static RGResourceUsage ConstantBufferView(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
							   DescriptorCreateHelper::ConstantBufferView(resource.GetCreateInfo())};
	}
	static RGResourceUsage VertexBufferView(RGResourceRef const& resource, uint32_t strideInBytes)
	{
		return RGResourceUsage{
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			DescriptorCreateHelper::VertexBufferView(resource.GetCreateInfo(),
													 DescriptorCreateHelper::Details<VertexBufferViewDesc>{
														 .VertexBuffer = {.StrideInBytes = strideInBytes}})};
	}
	static RGResourceUsage IndexBufferView(RGResourceRef const& resource, DXGI_FORMAT format)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_INDEX_BUFFER,
							   DescriptorCreateHelper::IndexBufferView(IndexBufferViewDesc{
								   .SizeInBytes = UINT(resource.GetCreateInfo().Desc.Width), .Format = format})};
	}
	static RGResourceUsage RenderTargetView(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_RENDER_TARGET,
							   DescriptorCreateHelper::RenderTargetView(resource.GetCreateInfo())};
	}
	static RGResourceUsage DepthStencilWrite(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_DEPTH_WRITE,
							   DescriptorCreateHelper::DepthStencilView(resource.GetCreateInfo())};
	}
	static RGResourceUsage DepthStencilRead(RGResourceRef const& resource)
	{
		return RGResourceUsage{D3D12_RESOURCE_STATE_DEPTH_READ,
							   DescriptorCreateHelper::DepthStencilView(resource.GetCreateInfo())};
	}
};

using RGResourceDescriptor = RGFuture<ResourceDescriptor, true>;

struct RGResourceViewBase : RGResourceRef
{
	RGResourceViewBase(RGResourceRef& resourceRef, std::deque<Ref<RGResourceDescriptor>> descriptors)
		: RGResourceRef(resourceRef), Descriptors(std::move(descriptors))
	{
	}

	template <typename T>
		requires std::is_same_v<T, RenderTargetViewDesc> || std::is_same_v<T, DepthStencilViewDesc> ||
				 std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
				 std::is_same_v<T, ConstantBufferViewDesc> || std::is_same_v<T, VertexBufferViewDesc> ||
				 std::is_same_v<T, IndexBufferViewDesc>
	auto& AsCPUDescriptor(size_t index = 0) const
	{
		return Descriptor(index).AsCPUDescriptor<T>();
	}

	template <typename T>
		requires std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
				 std::is_same_v<T, ConstantBufferViewDesc>
	auto& AsGPUDescriptor(size_t index = 0) const
	{
		return Descriptor(index).AsGPUDescriptor<T>();
	}

	ResourceDescriptor& Descriptor(size_t index = 0) const { return Descriptors[index]->Get(); }

	operator PoolResourceView() const { return GetResource(); }
	PoolResourceView operator->() const { return GetResource(); }

	std::deque<Ref<RGResourceDescriptor>> Descriptors;
};

struct RenderPassBuilder;
struct RGBInputResource;
struct RenderGraphBuilder;

struct RGBOutputResource : RGResourceRef
{
	RGBOutputResource(RGResourceRef resourceRef, std::string name, RenderPassBuilder& ownerPass)
		: RGResourceRef(resourceRef), Name(std::move(name)), OwnerPass(ownerPass)
	{
	}

	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	std::vector<Ref<RGBInputResource>> ConnectedInputs;
	void AddInput(RGBInputResource& input) { ConnectedInputs.push_back(input); }
};

struct RGBInputResource
{
	RGBInputResource(std::string name,
					 RenderPassBuilder& ownerPass,
					 RGBOutputResource& source,
					 D3D12_RESOURCE_STATES state)
		: Name(std::move(name)), OwnerPass(ownerPass), Source(source), State(state)
	{
	}

	Ref<RGResourceDescriptor> AddDescriptor(DescriptorDesc desc);

	std::string Name;
	Ref<RenderPassBuilder> OwnerPass;
	Ref<RGBOutputResource> Source;
	D3D12_RESOURCE_STATES State;
	std::deque<Ref<RGResourceDescriptor>> Descriptors;

	RGResourceViewBase GetResourceView() { return RGResourceViewBase{Source, Descriptors}; }
	operator RGResourceViewBase() { return GetResourceView(); }
	ResourcePool::Resource* operator->() { return GetResourceView().GetResource().operator->(); }
};

struct RenderPassBuilder
{
	RenderPassBuilder(std::string name, RenderGraphBuilder& rgBuilder) : Name(std::move(name)), RGBuilder(rgBuilder) {}

	std::string Name;
	std::function<void(CommandContext&)> Execute;
	Ref<RenderGraphBuilder> RGBuilder;

	Ref<RGBInputResource> AddInput(std::string name, RGBOutputResource& resource, RGResourceUsage usage);
	std::pair<Ref<RGBInputResource>, Ref<RGBOutputResource>> AddInOutResource(std::string name,
																			  RGBOutputResource& resource,
																			  RGResourceUsage usage);
	Ref<RGBInputResource> AddInResourceSetOut(std::string name,
											  Ref<RGBOutputResource>& resource,
											  RGResourceUsage usage);

	friend struct RenderGraphBuilder;

protected:
	std::deque<RGBInputResource> Inputs;
	std::deque<RGBOutputResource> Outputs;
};

struct RGResourceManager
{
	std::deque<PoolResourceView> ExternalResources;
	std::deque<RGGraphResource> GraphResources;
	struct ResourceInfo
	{
		D3D12_RESOURCE_STATES LastState{};
	};
	std::unordered_map<RGResourceRef, ResourceInfo> CreatedGraphResourcesMap;
	std::unordered_map<std::pair<RGResourceRef, DescriptorDesc>, RGResourceDescriptor> ResourceDescriptors;
	D3D12_RESOURCE_STATES& GetLastState(RGResourceRef const& resource);

	friend struct RenderGraphBuilder;

protected:
	void CreateResourcesAndDescriptors(ResourcePool& resourcePool);
	void FreeResources(ResourcePool& resPool);
	PoolResourceView& AddExternalResource(PoolResourceView resource);
	RGGraphResource& AddGraphResource(ResourceCreateInfo createInfo, std::string name);
	RGResourceDescriptor& GetDescriptor(RGResourceRef const& resource, DescriptorDesc const& desc);
};

struct RenderGraphBuilder
{
	RGResourceManager ResourceManager;
	std::deque<RenderPassBuilder> Passes;

	RenderGraphBuilder();

	RenderPassBuilder& AddPass(std::string name) { return Passes.emplace_back(std::move(name), *this); }
	Ref<RGBOutputResource> AddGraphResource(std::string name, ResourceCreateInfo createInfo);
	Ref<RGBOutputResource> GetOrAddExternalResource(PoolResourceView externalResource);

	void BuildAndExecute(ResourcePool& resourcePool, CommandContext& cmd);
	Ref<RGBInputResource> AddInputToPass(RenderPassBuilder& pass,
										 std::string name,
										 RGBOutputResource& resource,
										 RGResourceUsage usage);
	std::pair<RGBInputResource&, Ref<RGBOutputResource>> AddInOutToPass(RenderPassBuilder& pass,
																		std::string name,
																		RGBOutputResource& resourceRef,
																		RGResourceUsage usage);
	RGResourceDescriptor& AddDescriptorToInput(RenderPassBuilder& pass, RGBInputResource& input, DescriptorDesc desc);

private:
	Ref<RGBOutputResource> AddOutputToPass(RenderPassBuilder& pass, std::string name, RGResourceRef ref);
	Ref<RGBOutputResource> InitializeResourceProvider(std::string name, RGResourceRef resourceRef);
	void ExecutePass(RenderPassBuilder& pass, CommandContext& cmd);
	std::unordered_map<PoolResourceView, Ref<RGBOutputResource>> ResourceToLastOutput;
};
}; // namespace rad