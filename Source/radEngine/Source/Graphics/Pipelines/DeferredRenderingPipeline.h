#pragma once

#include "Graphics/DXResource.h"
#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"

#include "Graphics/RendererCommon.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/ResourcePool.h"

namespace rad
{

struct PreRenderPassData
{
	RenderGraphBuilder& GraphBuilder;
	const SceneRenderData& Frame;
};

struct ShadowMapPassData
{
	RenderGraphBuilder& GraphBuilder;
	const SceneRenderData& Frame;
	Ref<RGBOutputResource>& OutShadowMap;
};

struct DeferredPassData
{
	RenderGraphBuilder& GraphBuilder;
	const SceneRenderData& Frame;
	Ref<RGBOutputResource>& OutAlbedoBuffer;
	Ref<RGBOutputResource>& OutNormalBuffer;
	Ref<RGBOutputResource>& OutDepthBuffer;
};

struct WaterPassData
{
	RenderGraphBuilder& GraphBuilder;
	const SceneRenderData& Frame;
	Ref<RGBOutputResource>& OutReflectionRefraction;
	Ref<RGBOutputResource>& OutDepthBuffer;
	Ref<RGBOutputResource>& InViewTransform;
};

struct ForwardPassData
{
	RenderGraphBuilder& GraphBuilder;
	const SceneRenderData& Frame;
	Ref<RGBOutputResource>& InOutColor;
	Ref<RGBOutputResource>& InOutSSDepth;
	Ref<RGBOutputResource>& InViewTransform;
	Ref<RGBOutputResource>& InOpaquaDepth;
	Ref<RGBOutputResource>& InReflectionResult;
	Ref<RGBOutputResource>& InRefractionResult;
};

struct DeferredRenderingPipeline
{
	DeferredRenderingPipeline(rad::Renderer& renderer) : Renderer(renderer) {}
	bool Setup();
	bool OnResize(uint32_t width, uint32_t height);

	Ref<RGBOutputResource> BuildFrameRenderGraph(RenderGraphBuilder& graphBuilder, SceneRenderData& sceneData);

	std::queue<std::move_only_function<void(PreRenderPassData&)>> EnqueuedPreRenderFuncs;

	Event<PreRenderPassData&> OnPreRenderPass;
	Event<ShadowMapPassData&> OnShadowMapPass;
	Event<DeferredPassData&> OnDeferredPass;
	Event<WaterPassData&> OnWaterPass;
	Event<ForwardPassData&> OnForwardRenderPass;

private:
	Renderer& Renderer;

	ComputePipelineState<hlsl::ScreenSpaceRaymarchResources> ScreenSpaceRaymarchPipelineState{};
	GraphicsPipelineState<hlsl::LightingResources> LightingPipelineState{};

	DescriptorAllocation ShadowMapSampler{};
	D3D12_VIEWPORT ShadowMapViewport{};
	D3D12_VIEWPORT Viewport{};
	D3D12_RECT ScissorRect{};
};

} // namespace rad