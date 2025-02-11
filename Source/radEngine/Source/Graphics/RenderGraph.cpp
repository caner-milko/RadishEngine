#include "RenderGraph.h"
#include "DXResource.h"

namespace rad
{
void Test()
{
	DXResource* vertexBuffer = nullptr;
	DXResource* indexBuffer = nullptr;

	RenderGraphBuilder builder;
	auto& shadowMap = builder.AddGraphResource(
		"Shadow Map",
		RGResourceCreateInfo{/*.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, .Width = 1024, .Height = 1024*/});

	auto& staticVertexBuffer =
		builder.AddExternalResource("Static Mesh Vertex Buffer", *vertexBuffer,
									RGResourceUsage{/*.State = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER*/});
	auto& staticIndexBuffer =
		builder.AddExternalResource("Static Mesh Index Buffer", *indexBuffer,
									RGResourceUsage{/*.State = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER*/});

	auto& shadowPass = builder.AddPass("Static Mesh Shadow Pass");
	auto [depthBufShadowMapIn, depthBufShadowMapOut] =
		shadowPass.AddInOutResource("Shadow Map", shadowMap, RGResourceUsage{/*.State = D3D12_RESOURCE_STATE_DEPTH_WRITE*/});

	shadowPass.Execute = [
		RAD_RENDER_PASS_RESOURCE_TEX(shadowMap, depthBufShadowMapIn),
		 RAD_RENDER_PASS_RESOURCE_BUF(vertexBuf, shadowPass.AddInput("Vertex Buffer", staticVertexBuffer, RGResourceUsage{})),
		 RAD_RENDER_PASS_RESOURCE_BUF(indexBuf, shadowPass.AddInput("Index Buffer", staticIndexBuffer, RGResourceUsage{}))
	](CommandContext& cmd) {
		// Draw static meshes to shadow map
		D3D12_VIEWPORT vp = {};
		//cmd->RSSetViewports(1, shadowMap->GetCreateInfo()
	};

	auto& gbufferDepth = builder.AddGraphResource("GBuffer Depth", RGResourceCreateInfo{});
	auto& gbufferAlbedo = builder.AddGraphResource("GBuffer Albedo", RGResourceCreateInfo{});
	auto& gbufferNormal = builder.AddGraphResource("GBuffer Normal", RGResourceCreateInfo{});

	auto& mainPass = builder.AddPass("Static Mesh Deferred Render Pass");
	auto& vbBuf = mainPass.AddInput("Vertex Buffer", staticVertexBuffer, RGResourceUsage{});
	auto& ibBuf = mainPass.AddInput("Index Buffer", staticIndexBuffer, RGResourceUsage{});
	auto [depthBufIn, depthBufOut] = mainPass.AddInOutResource("GBuffer Depth", gbufferDepth, RGResourceUsage{});
	auto [albedoBufIn, albedoBufOut] = mainPass.AddInOutResource("GBuffer Albedo", gbufferAlbedo, RGResourceUsage{});
	auto [normalBufIn, normalBufOut] = mainPass.AddInOutResource("GBuffer Normal", gbufferNormal, RGResourceUsage{});
	mainPass.Execute = [vbBuf = Ref(vbBuf), ibBuf = Ref(ibBuf), depthBuf = Ref(depthBufIn),
						albedoBuf = Ref(albedoBufIn), normalBuf = Ref(normalBufIn)](CommandContext& ctx)
	{
		// Draw static meshes to GBuffer
	};
}
RGBOutputResource& RenderGraphBuilder::AddGraphResource(std::string name, ResourceCreateInfo createInfo)
{
	auto& resource = ResourceManager.GraphResources.emplace_back(name, createInfo);
	return InitializeResourceProvider(std::move(name), resource);
}
RGBOutputResource& RenderGraphBuilder::AddExternalResource(std::string name, DXResource& resource,
														   ResourceCreateInfo createInfo,
														   RGResourceUsage initialUsage)
{
	auto& externalResource = ResourceManager.ExternalResources.emplace_back(name, resource, std::move(createInfo), std::move(initialUsage));
	return InitializeResourceProvider(std::move(name), externalResource);
}
RGBOutputResource& RenderGraphBuilder::InitializeResourceProvider(std::string name, RGResourceRef resourceRef)
{
	auto& providerPass = AddPass(std::move(name) + " Provider");
	auto& outRef = providerPass.Outputs.emplace_back(std::move(name), providerPass, resourceRef);
	return outRef;
}
RGBInputResource& RenderPassBuilder::AddInput(std::string name, RGBOutputResource& output, RGResourceUsage usage)
{
	auto& descriptorRef = RGBuilder->ResourceManager.GetDescriptor(output.ResourceRef, usage.DescriptorDesc);
	auto& inRef = Inputs.emplace_back(std::move(name), *this, output, std::move(usage), std::move(usage), descriptorRef);
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

void RenderGraphBuilder::Build(Renderer& renderer, CommandContext& cmd)
{
	/*
		1. Create all graph resources
		2. Create descriptors
		3. Start from the leftmost & start recording passes/barriers
	*/

}

} // namespace rad