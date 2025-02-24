#include "RenderGraph.h"
#include "DXResource.h"
#include "ResourcePool.h"
#include "Renderer.h"

namespace rad
{
RGBOutputResource& RenderGraphBuilder::AddGraphResource(std::string name, ResourceCreateInfo createInfo)
{
	auto& resource = ResourceManager.AddGraphResource(std::move(createInfo), name);
	return InitializeResourceProvider(std::move(name), resource);
}

RGBOutputResource& RenderGraphBuilder::GetOrAddExternalResource(PoolResourceView resource)
{
	if (auto it = ResourceToLastOutput.find(resource); it != ResourceToLastOutput.end())
		return it->second;
	auto& externalResource = ResourceManager.AddExternalResource(resource);
	return InitializeResourceProvider(resource.GetName(), externalResource);
}

RGBOutputResource& RenderGraphBuilder::InitializeResourceProvider(std::string name, RGResourceRef resourceRef)
{
	auto& providerPass = AddPass(std::move(name) + " Provider");
	return AddOutputToPass(providerPass, std::move(name), resourceRef);
}

RGBInputResource& RenderPassBuilder::AddInput(std::string name, RGBOutputResource& output, RGResourceUsage usage)
{
	return RGBuilder->AddInputToPass(*this, std::move(name), output, std::move(usage));
}

std::pair<RGBInputResource&, RGBOutputResource&> RenderPassBuilder::AddInOutResource(std::string name,
																					 RGBOutputResource& output,
																					 RGResourceUsage usage)
{
	return RGBuilder->AddInOutToPass(*this, std::move(name), output, std::move(usage));
}

RGBInputResource& RenderPassBuilder::AddInResourceSetOut(
	std::string name,
																					 Ref<RGBOutputResource>& resource,
																					 RGResourceUsage usage)
{
	auto [in, out] = AddInOutResource(std::move(name), *resource, std::move(usage));
	resource = out;
	return in;
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
			auto& resInfo = ResourceManager.CreatedGraphResourcesMap[input.Source];
			auto& lastState = ResourceManager.GetLastState(input.GetResourceView());
			if (input.State != lastState)
			{
				D3D12_RESOURCE_BARRIER barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
												  .Transition = {.pResource = &input->DXRes,
																 .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
																 .StateBefore = resInfo.LastState,
																 .StateAfter = input.State}};
				barriers.push_back(barrier);
				lastState = input.State;
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
	ResourceManager.FreeResources(renderer);
}

RGBInputResource& RenderGraphBuilder::AddInputToPass(RenderPassBuilder& pass, std::string name,
													 RGBOutputResource& fromOut, RGResourceUsage usage)
{
	auto& inRef = pass.Inputs.emplace_back(std::move(name), pass, fromOut, usage.State);
	fromOut.ConnectedInputs.push_back(inRef);
	for (auto& desc : usage.DescriptorDescs)
		AddDescriptorToInput(pass, inRef, desc);
	return inRef;
}

RGBOutputResource& RenderGraphBuilder::AddOutputToPass(RenderPassBuilder& pass, std::string name, RGResourceRef ref) 
{
	auto& outputRef = pass.Outputs.emplace_back(ref, std::move(name), pass);
	if (auto* poolRes = ref.AsExternalResource())
		ResourceToLastOutput.insert_or_assign(*poolRes, outputRef);
	return outputRef;
}

std::pair<RGBInputResource&, RGBOutputResource&> RenderGraphBuilder::AddInOutToPass(RenderPassBuilder& pass,
																					std::string name,
																					RGBOutputResource& fromOut,
																					RGResourceUsage usage)
{
	auto& input = AddInputToPass(pass, name, fromOut, std::move(usage));
	auto& output = AddOutputToPass(pass, name, fromOut.GetResource());
	return {input, output};
}

RGResourceDescriptor& RenderGraphBuilder::AddDescriptorToInput(RenderPassBuilder& pass, RGBInputResource& input,
															   DescriptorDesc desc)
{
	return input.Descriptors.emplace_back(ResourceManager.GetDescriptor(input.Source, std::move(desc)));
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
	for (auto [resource, resInfo] : CreatedGraphResourcesMap)
		resource.GetResource()->State = resInfo.LastState;
	for (auto& resource : GraphResources)
		renderer.ResourcePool->FreeResource(resource.Get());
}

} // namespace rad