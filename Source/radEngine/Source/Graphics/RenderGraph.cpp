#include "RenderGraph.h"
#include "DXResource.h"
#include "ResourcePool.h"
#include "Renderer.h"

namespace rad
{
void Test()
{
	PoolResourceView* vertexBuffer = nullptr;
	PoolResourceView* indexBuffer = nullptr;

	RenderGraphBuilder builder;
	auto& shadowMap = builder.AddGraphResource(
		"Shadow Map",
		ResourceCreateInfo{.Desc = {.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, .Width = 1024, .Height = 1024}});

	auto& staticVertexBuffer = builder.AddExternalResource(*vertexBuffer);
	auto& staticIndexBuffer = builder.AddExternalResource(*indexBuffer);

	auto& shadowPass = builder.AddPass("Static Mesh Shadow Pass");
	auto [depthBufShadowMapIn, depthBufShadowMapOut] = shadowPass.AddInOutResource(
		"Shadow Map", shadowMap,
		RGResourceUsage{.State = D3D12_RESOURCE_STATE_DEPTH_WRITE,
						.DescriptorDesc = CPUDescriptorDesc{DepthStencilViewDesc{D3D12_DEPTH_STENCIL_VIEW_DESC{
							.Format = DXGI_FORMAT_D32_FLOAT, .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D}}}});

	shadowPass.Execute =
		[shadowMap = RGResourceViewBase(depthBufShadowMapIn),
		 vertexBuf =
			 RGResourceViewBase(shadowPass.AddInput("Vertex Buffer", staticVertexBuffer,
													RGResourceUsage{D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
																	CPUDescriptorDesc{VertexBufferViewDesc{}}})),
		 indexBuf = RGResourceViewBase(shadowPass.AddInput(
			 "Index Buffer", staticIndexBuffer,
			 RGResourceUsage{D3D12_RESOURCE_STATE_INDEX_BUFFER, CPUDescriptorDesc{IndexBufferViewDesc{}}}))](
			CommandContext& cmd)
	{
		// Draw static meshes to shadow map
		D3D12_VIEWPORT vp = {};
		// cmd->RSSetViewports(1, shadowMap->GetCreateInfo()
	};

	auto& gbufferDepth = builder.AddGraphResource("GBuffer Depth", ResourceCreateInfo{});
	auto& gbufferAlbedo = builder.AddGraphResource("GBuffer Albedo", ResourceCreateInfo{});
	auto& gbufferNormal = builder.AddGraphResource("GBuffer Normal", ResourceCreateInfo{});

	auto& mainPass = builder.AddPass("Static Mesh Deferred Render Pass");
	auto& vbBuf = mainPass.AddInput("Vertex Buffer", staticVertexBuffer, RGResourceUsage{});
	auto& ibBuf = mainPass.AddInput("Index Buffer", staticIndexBuffer, RGResourceUsage{});
	auto [depthBufIn, depthBufOut] = mainPass.AddInOutResource("GBuffer Depth", gbufferDepth, RGResourceUsage{});
	auto [albedoBufIn, albedoBufOut] = mainPass.AddInOutResource("GBuffer Albedo", gbufferAlbedo, RGResourceUsage{});
	auto [normalBufIn, normalBufOut] = mainPass.AddInOutResource("GBuffer Normal", gbufferNormal, RGResourceUsage{});
	mainPass.Execute = [vbBuf = RGResourceViewBase(vbBuf), ibBuf = RGResourceViewBase(ibBuf),
						depthBuf = Ref(depthBufIn), albedoBuf = RGResourceViewBase(albedoBufIn),
						normalBuf = RGResourceViewBase(normalBufIn)](CommandContext& ctx)
	{
		// Draw static meshes to GBuffer
	};
}

RGBOutputResource& RenderGraphBuilder::AddGraphResource(std::string name, ResourceCreateInfo createInfo)
{
	auto& resource = ResourceManager.AddGraphResource(std::move(createInfo), name);
	return InitializeResourceProvider(std::move(name), resource);
}

RGBOutputResource& RenderGraphBuilder::AddExternalResource(PoolResourceView resource)
{
	auto& externalResource = ResourceManager.AddExternalResource(resource);
	return InitializeResourceProvider(resource.GetName(), externalResource);
}

RGBOutputResource& RenderGraphBuilder::InitializeResourceProvider(std::string name, RGResourceRef resourceRef)
{
	auto& providerPass = AddPass(std::move(name) + " Provider");
	auto& outRef = providerPass.Outputs.emplace_back(std::move(name), providerPass, resourceRef);
	return outRef;
}

RGBInputResource& RenderPassBuilder::AddInput(std::string name, RGBOutputResource& output, RGResourceUsage usage)
{
	auto& rgDesc = RGBuilder->ResourceManager.GetDescriptor(output.ResourceRef, usage.DescriptorDesc);
	auto& inRef = Inputs.emplace_back(std::move(name), *this, output, std::move(usage), rgDesc);
	output.ConnectedInputs.push_back(inRef);
	return inRef;
}

std::pair<RGBInputResource&, RGBOutputResource&> RenderPassBuilder::AddInOutResource(std::string name,
																					 RGBOutputResource& output,
																					 RGResourceUsage usage)
{
	auto& input = AddInput(name, output, std::move(usage));
	auto& outputRef = Outputs.emplace_back(std::move(name), *this, output.ResourceRef);
	return {input, output};
}

void RenderGraphBuilder::BuildAndExecute(Renderer& renderer, CommandContext& cmd)
{
	/*
		1. Create all graph resources && descriptors
		2. Start from the leftmost & start recording passes/barriers
	*/

	// 1. Create all graph resources
	ResourceManager.CreateResourcesAndDescriptors(renderer);

	// 2. Start from the leftmost & start recording passes/barriers
	std::unordered_set<Ref<RenderPassBuilder>> visitedPasses;
	std::queue<Ref<RenderPassBuilder>> passQueue;
	// Start from leftmost paths
	for (auto& pass : Passes)
		if (pass.Inputs.empty())
			passQueue.push(pass);

	while (!passQueue.empty())
	{
		RenderPassBuilder& pass = passQueue.front();
		visitedPasses.insert(pass);
		passQueue.pop();
		// Record pass
		//cmd.BeginPass(pass.Name);

		std::vector<D3D12_RESOURCE_BARRIER> barriers;

		for (auto& input : pass.Inputs)
		{
			auto& resInfo = ResourceManager.CreatedGraphResourcesMap[input.Source->ResourceRef];
			auto& lastState = ResourceManager.GetLastState(input.GetResourceView());
			if (input.Usage.State != lastState)
			{
				D3D12_RESOURCE_BARRIER barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
												  .Transition = {.pResource = &input->DXRes,
																 .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
																 .StateBefore = resInfo.LastState,
																 .StateAfter = input.Usage.State}};
				barriers.push_back(barrier);
				lastState = input.Usage.State;
			}
		}
		if (!barriers.empty())
			cmd->ResourceBarrier(barriers.size(), barriers.data());
		if (pass.Execute)
			pass.Execute(cmd);
		//cmd.EndPass();
		// Add outputs to queue
		for (auto& output : pass.Outputs)
		{
			for (auto& input : output.ConnectedInputs)
			{
				RenderPassBuilder& owner = input->OwnerPass;
				bool canVisit = true;
				for (auto& input : owner.Inputs)
					if (!visitedPasses.contains(input.Source->OwnerPass))
					{
						canVisit = false;
						break;
					}
				if (canVisit)
					passQueue.push(owner);
			}
		}
	}
}

PoolResourceView& RGResourceManager::AddExternalResource(PoolResourceView resource)
{
	auto& resPoolRef = ExternalResources.emplace_back(resource);
	CreatedGraphResourcesMap[resPoolRef];
	return resPoolRef;
}

RGGraphResource& RGResourceManager::AddGraphResource(ResourceCreateInfo createInfo, std::string name)
{
	auto& res = GraphResources.emplace_back(createInfo, std::move(name));
	CreatedGraphResourcesMap[res];
	return res;
}

RGResourceDescriptor& RGResourceManager::GetDescriptor(RGResourceRef const& resource, DescriptorDesc const& desc)
{
	return CreatedGraphResourcesMap[resource].Descriptors.try_emplace(desc, RGResourceDescriptor{}).first->second;
}

D3D12_RESOURCE_STATES& RGResourceManager::GetLastState(RGResourceRef const& resource)
{
	return CreatedGraphResourcesMap[resource].LastState;
}

void RGResourceManager::CreateResourcesAndDescriptors(Renderer& renderer)
{
	// Create all graph resources
	for (auto& resource : GraphResources)
		resource.ResourceDecided(renderer.ResourcePool->GetResource(resource.CreateInfo, resource.Name));
	// Create all descriptors
	for (auto& [resourceRef, resInfo] : CreatedGraphResourcesMap)
	{
		resInfo.LastState = resourceRef.GetResource()->State;
		for (auto& [desc, rgDesc] : resInfo.Descriptors)
			rgDesc.ResourceDecided(renderer.ResourcePool->GetDescriptor(resourceRef.GetResource(), desc));
	}
}

void RGResourceManager::FreeResources(Renderer& renderer) 
{
	for (auto& resource : GraphResources)
		renderer.ResourcePool->FreeResource(resource.Get());
}

} // namespace rad