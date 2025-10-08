#include "RenderGraph.h"
#include "DXResource.h"
#include "ResourcePool.h"
#include <sstream>

namespace rad
{
Ref<RGBOutputResource> RenderGraphBuilder::AddGraphResource(std::string name, ResourceCreateInfo createInfo)
{
	auto& resource = ResourceManager.AddGraphResource(std::move(createInfo), name);
	return InitializeResourceProvider(std::move(name), resource);
}

Ref<RGBOutputResource> RenderGraphBuilder::GetOrAddExternalResource(PoolResourceView resource)
{
	if (auto it = ResourceToLastOutput.find(resource); it != ResourceToLastOutput.end())
		return it->second;
	auto& externalResource = ResourceManager.AddExternalResource(resource);
	return InitializeResourceProvider(resource.GetName(), externalResource);
}

Ref<RGBOutputResource> RenderGraphBuilder::InitializeResourceProvider(std::string name, RGResourceRef resourceRef)
{
	auto& providerPass = AddPass(name + " Provider");
	return AddOutputToPass(providerPass, std::move(name), resourceRef);
}

Ref<RGBInputResource> RenderPassBuilder::AddInput(std::string name, RGBOutputResource& output, RGResourceUsage usage)
{
	return RGBuilder->AddInputToPass(*this, std::move(name), output, std::move(usage));
}

std::pair<Ref<RGBInputResource>, Ref<RGBOutputResource>> RenderPassBuilder::AddInOutResource(std::string name,
																							 RGBOutputResource& output,
																							 RGResourceUsage usage)
{
	return RGBuilder->AddInOutToPass(*this, std::move(name), output, std::move(usage));
}

Ref<RGBInputResource> RenderPassBuilder::AddInResourceSetOut(std::string name,
															 Ref<RGBOutputResource>& resource,
															 RGResourceUsage usage)
{
	auto [in, out] = AddInOutResource(std::move(name), *resource, std::move(usage));
	resource = out;
	return in;
}

void RenderGraphBuilder::BuildAndExecute(ResourcePool& resourcePool, CommandContext& cmd)
{
#ifndef NDEBUG
	for (auto& pass : Passes)
	{
		for (auto& input : pass.Inputs)
		{
			// assert(input.Source->OwnerPass != pass);
			auto& sourceConnecteds = input.Source->ConnectedInputs;
			assert(std::find(sourceConnecteds.begin(), sourceConnecteds.end(), Ref(input)) != sourceConnecteds.end() &&
				   "Input not registered in source's connected inputs");
		}
	}
#endif

	/*
		1. Create all graph resources && descriptors
		2. Start from the leftmost & start recording passes/barriers
	*/

	// 1. Create all graph resources
	ResourceManager.CreateResourcesAndDescriptors(resourcePool);

	std::unordered_map<Ref<RenderPassBuilder>, uint64_t> passRemainingDepCount;
	{
		// Count dependencies for each pass
		for (auto& pass : Passes)
		{
			uint64_t depCount = 0;

			for (auto& input : pass.Inputs)
				if (input.Source->OwnerPass != pass)
					depCount++;

			passRemainingDepCount.insert_or_assign(pass, depCount);
		}
	}

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
		passQueue.pop();
		if (!visitedPasses.insert(pass).second)
			continue;
		ExecutePass(pass, cmd);
		for (auto& output : pass.Outputs)
		{
			for (auto& input : output.ConnectedInputs)
			{
				RenderPassBuilder& owner = input->OwnerPass;
				if (&owner == &pass)
					continue;
				auto it = passRemainingDepCount.find(owner);
				assert(it != passRemainingDepCount.end());
				if (it == passRemainingDepCount.end())
					continue;
				assert(it->second > 0);
				if (--it->second == 0)
					passQueue.push(owner);
			}
		}
	}
#ifndef NDEBUG
	if (visitedPasses.size() != Passes.size())
	{
		std::stringstream ss;
		ss << "RenderGraphBuilder: Not all passes were visited. Unvisited passes:\n";
		for (auto& pass : Passes)
			if (!visitedPasses.contains(pass))
				ss << " - " << pass.Name << "\n";
		std::cerr << ss.str();
		assert(false);
	}
#endif

	ResourceManager.FreeResources(resourcePool);
}

void RenderGraphBuilder::ExecutePass(RenderPassBuilder& pass, CommandContext& cmd)
{
	// Record pass
	// cmd.BeginPass(pass.Name);

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
	// cmd.EndPass();
	//  Add outputs to queue
}

Ref<RGBInputResource> RenderGraphBuilder::AddInputToPass(RenderPassBuilder& pass,
														 std::string name,
														 RGBOutputResource& fromOut,
														 RGResourceUsage usage)
{
	auto& inRef = pass.Inputs.emplace_back(std::move(name), pass, fromOut, usage.State);
	fromOut.ConnectedInputs.push_back(inRef);
	for (auto& desc : usage.DescriptorDescs)
		AddDescriptorToInput(pass, inRef, desc);
	return inRef;
}

Ref<RGBOutputResource> RenderGraphBuilder::AddOutputToPass(RenderPassBuilder& pass, std::string name, RGResourceRef ref)
{
	auto& outputRef = pass.Outputs.emplace_back(ref, std::move(name), pass);
	if (auto* poolRes = ref.AsExternalResource())
		ResourceToLastOutput.insert_or_assign(*poolRes, outputRef);
	return outputRef;
}

std::pair<RGBInputResource&, Ref<RGBOutputResource>> RenderGraphBuilder::AddInOutToPass(RenderPassBuilder& pass,
																						std::string name,
																						RGBOutputResource& fromOut,
																						RGResourceUsage usage)
{
	auto input = AddInputToPass(pass, name, fromOut, std::move(usage));
	auto output = AddOutputToPass(pass, name, fromOut);
	return {input, output};
}

RGResourceDescriptor& RenderGraphBuilder::AddDescriptorToInput(RenderPassBuilder& pass,
															   RGBInputResource& input,
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

void RGResourceManager::CreateResourcesAndDescriptors(ResourcePool& resourcePool)
{
	// Create all graph resources
	for (auto& resource : GraphResources)
		resource.ResourceDecided(resourcePool.GetResource(resource.CreateInfo, resource.Name));
	// Create all descriptors
	for (auto& [resourceRef, resInfo] : CreatedGraphResourcesMap)
	{
		resInfo.LastState = resourceRef.GetResource()->State;
		for (auto& [desc, rgDesc] : resInfo.Descriptors)
		{
			rgDesc.ResourceDecided(resourcePool.GetDescriptor(resourceRef.GetResource(), desc));
			assert(!rgDesc.Get().valueless_by_exception());
		}
	}
}

void RGResourceManager::FreeResources(ResourcePool& resourcePool)
{
	for (auto [resource, resInfo] : CreatedGraphResourcesMap)
		resource.GetResource()->State = resInfo.LastState;
	for (auto& resource : GraphResources)
		resourcePool.FreeResource(resource.Get());
}

void TestGraph()
{
	rad::RadDevice* device = nullptr;
	ResourcePool resourcePool(*device);
	RenderGraphBuilder graphBuilder{};
	auto testTex = graphBuilder.AddGraphResource(
		"TestTex",
		ResourceCreateHelper::Texture2D(
			1024,
			1024,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			ResourcePresetFlags::RenderTarget,
			ResourceCreateHelper::TextureDetails{.DetailedFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET}));

	auto& indicesRes = resourcePool.GetResource(
		ResourceCreateHelper::Buffer(256 * sizeof(uint32_t), ResourcePresetFlags::IndexBuffer), "TestBuffer");

	auto& readTex =
		resourcePool.GetResource(ResourceCreateHelper::Texture2D(
									 512,
									 512,
									 DXGI_FORMAT_R8G8B8A8_UNORM,
									 ResourcePresetFlags::ShaderResource,
									 ResourceCreateHelper::TextureDetails{.DetailedFlags = D3D12_RESOURCE_FLAG_NONE}),
								 "ReadTex");

	auto externalReadTex = graphBuilder.GetOrAddExternalResource(readTex.AsView());
	auto externalBuf = graphBuilder.GetOrAddExternalResource(indicesRes.AsView());
	auto& pass1 = graphBuilder.AddPass("TestPass1");
	auto inTestTex = pass1.AddInResourceSetOut("InOutTestTex", testTex, RGResourceUsage::RenderTargetView(*testTex));
	auto inReadTex =
		pass1.AddInput("InReadTex", *externalReadTex, RGResourceUsage::PixelShaderResourceView(*externalReadTex));
	auto inBuf = pass1.AddInput("InBuf", *externalBuf, RGResourceUsage::PixelShaderResourceView(*externalBuf));
	pass1.Execute = [inTestTex, inBuf, inReadTex](CommandContext& cmd) {
		auto rtv = inTestTex->GetResourceView().AsCPUDescriptor<RenderTargetViewDesc>();
		auto srvTex = inReadTex->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>();
		auto srvBuf = inBuf->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>();
	};
}

Ref<RGResourceDescriptor> RGBInputResource::AddDescriptor(DescriptorDesc desc)
{
	return OwnerPass->RGBuilder->AddDescriptorToInput(OwnerPass, *this, std::move(desc));
}

} // namespace rad