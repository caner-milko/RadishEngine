#pragma once

#include "Graphics/DXResource.h"
#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"

#include "Graphics/RendererCommon.h"

namespace rad
{
struct DeferredRenderingPipeline
{
	DeferredRenderingPipeline(rad::Renderer& renderer) : Renderer(renderer) {}
	bool Setup();
	bool OnResize(uint32_t width, uint32_t height);

	DXTexture& GetOutputBuffer()
	{
		return OutputBuffer;
	}
	DescriptorAllocationView GetOutputBufferSRV()
	{
		return OutputBufferSRV.GetView();
	}
	DXTexture& GetShadowMap()
	{
		return ShadowMap;
	}
	DescriptorAllocationView GetShadowMapSRV()
	{
		return ShadowMapSRV.GetView();
	}
	DXTexture& GetAlbedoBuffer()
	{
		return AlbedoBuffer;
	}
	DescriptorAllocationView GetAlbedoBufferSRV()
	{
		return GBuffersSRV.GetView();
	}
	DXTexture& GetNormalBuffer()
	{
		return NormalBuffer;
	}
	DescriptorAllocationView GetNormalBufferSRV()
	{
		return GBuffersSRV.GetView(1);
	}

	void BeginFrame(CommandContext& cmdContext, RenderFrameRecord& frameRecord);
	void ShadowMapPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord);
	void DeferredRenderPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord);
	void WaterRenderPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord);
	void LightingPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord);
	void ForwardRenderPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord);
	void ScreenSpaceRaymarchPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord);

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
	DXTexture OutputBuffer{};
	DescriptorAllocation OutputBufferRTV{};
	DescriptorAllocation OutputBufferSRV{};

	D3D12_VIEWPORT ShadowMapViewport{};
	D3D12_VIEWPORT Viewport{};
	D3D12_RECT ScissorRect{};
};

} // namespace rad