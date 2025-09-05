#pragma once

#include "Graphics/DXResource.h"
#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"

#include "Graphics/RendererCommon.h"
#include "Graphics/RenderGraph.h"

namespace rad
{
struct FrameData
{
	uint64_t FrameNumber;
	float DeltaTime;
};

struct PreRenderPassData
{
	RenderGraphBuilder& GraphBuilder;
	const FrameData Frame;
};

struct ShadowMapPassData
{
	RenderGraphBuilder& GraphBuilder;
	const FrameData Frame;
	Ref<RGBOutputResource> OutShadowMap;
};

struct DeferredPassData
{
	RenderGraphBuilder& GraphBuilder;
	const FrameData Frame;
	Ref<RGBOutputResource> OutAlbedoBuffer;
	Ref<RGBOutputResource> OutNormalBuffer;
	Ref<RGBOutputResource> OutDepthBuffer;
};

struct WaterPassData
{
	RenderGraphBuilder& GraphBuilder;
	const FrameData Frame;
	Ref<RGBOutputResource> OutReflectionRefraction;
	Ref<RGBOutputResource> OutDepthBuffer;
	Ref<RGBOutputResource> InViewTransform;
};

struct ForwardPassData
{
	RenderGraphBuilder& GraphBuilder;
	const FrameData Frame;
	Ref<RGBOutputResource> InOutColor;
	Ref<RGBOutputResource> InOutSSDepth;
	Ref<RGBOutputResource> InViewTransform;
	Ref<RGBOutputResource> InOpaquaDepth;
	Ref<RGBOutputResource> InReflectionResult;
	Ref<RGBOutputResource> InRefractionResult;
};

struct DeferredRenderingPipeline
{
	DeferredRenderingPipeline(rad::Renderer& renderer) : Renderer(renderer) {}
	bool Setup();
	bool OnResize(uint32_t width, uint32_t height);

	DXTexture& GetOutputBuffer() { return OutputBuffer; }
	DescriptorAllocationView GetOutputBufferSRV() { return OutputBufferSRV.GetView(); }
	DXTexture& GetShadowMap() { return ShadowMap; }
	DescriptorAllocationView GetShadowMapSRV() { return ShadowMapSRV.GetView(); }
	DXTexture& GetAlbedoBuffer() { return AlbedoBuffer; }
	DescriptorAllocationView GetAlbedoBufferSRV() { return GBuffersSRV.GetView(); }
	DXTexture& GetNormalBuffer() { return NormalBuffer; }
	DescriptorAllocationView GetNormalBufferSRV() { return GBuffersSRV.GetView(1); }

	void BuildFrameRenderGraph(RenderGraphBuilder& graphBuilder, SceneRenderData& sceneData);

	Event<PreRenderPassData&> OnPreRenderPass;
	Event<ShadowMapPassData&> OnShadowMapPass;
	Event<DeferredPassData&> OnDeferredPass;
	Event<WaterPassData&> OnWaterPass;
	Event<ForwardPassData&> OnForwardRenderPass;

private:
	bool SetupLightingPass();
	bool SetupShadowMapPass();
	bool SetupScreenSpaceRaymarchPass();

	Renderer& Renderer;

	DXTexture DepthBuffer{};
	DXTexture AlbedoBuffer{};
	DXTexture NormalBuffer{};
	// RG - Reflection Normal, BA - Refraction Normal
	DXTexture SSReflectRefractBuffer{};
	DXTexture SSDepthBuffer{};

	DescriptorAllocation DepthBufferDSV{};
	DescriptorAllocation AlbedoBufferRTV{};
	DescriptorAllocation NormalBufferRTV{};
	DescriptorAllocation SSReflectRefractBufferRTV{};
	DescriptorAllocation SSDepthBufferDSV{};
	DescriptorAllocation GBuffersSRV{};

	DXTexture ShadowMap{};
	DescriptorAllocation ShadowMapDSV{};
	DescriptorAllocation ShadowMapSRV{};
	DescriptorAllocation ShadowMapSampler{};

	DXBuffer LightBuffer{};
	DescriptorAllocation LightBufferCBV{};
	DXTypedSingularBuffer<hlsl::ViewTransformBuffer> ViewTransformBuffer{};
	DescriptorAllocation ViewTransformBufferCBV{};

	ComputePipelineState<hlsl::ScreenSpaceRaymarchResources> ScreenSpaceRaymarchPipelineState{};
	// RG - Reflection UV, A - Visibility
	DXTexture ReflectionResultBuffer{};
	DescriptorAllocation ReflectionResultBufferUAV{};
	DescriptorAllocation ReflectionResultBufferSRV{};
	// RG - Refraction UV, A - Visibility
	DXTexture RefractionResultBuffer{};
	DescriptorAllocation RefractionResultBufferUAV{};
	DescriptorAllocation RefractionResultBufferSRV{};

	GraphicsPipelineState<hlsl::LightingResources> LightingPipelineState{};
	DXTexture LightingResultBuffer{};
	DescriptorAllocation LightingResultBufferRTV{};
	DescriptorAllocation LightingResultBufferSRV{};
	DXTexture OutputBuffer{};
	DescriptorAllocation OutputBufferRTV{};
	DescriptorAllocation OutputBufferSRV{};

	D3D12_VIEWPORT ShadowMapViewport{};
	D3D12_VIEWPORT Viewport{};
	D3D12_RECT ScissorRect{};
};

} // namespace rad