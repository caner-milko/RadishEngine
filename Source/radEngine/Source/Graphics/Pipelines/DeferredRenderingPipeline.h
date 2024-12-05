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


		DXTexture& GetOutputBuffer() { return OutputBuffer; }
		DescriptorAllocationView GetOutputBufferSRV() { return OutputBufferSRV.GetView(); }
		DXTexture& GetShadowMap() { return ShadowMap; }
		DescriptorAllocationView GetShadowMapSRV() { return ShadowMapSRV.GetView(); }
		DXTexture& GetAlbedoBuffer() { return AlbedoBuffer; }
		DescriptorAllocationView GetAlbedoBufferSRV() { return GBuffersSRV.GetView(); }
		DXTexture& GetNormalBuffer() { return NormalBuffer; }
		DescriptorAllocationView GetNormalBufferSRV() { return GBuffersSRV.GetView(1); }

		void ShadowMapPass(CommandContext cmdContext, RenderFrameRecord& frameRecord);
		void DeferredRenderPass(CommandContext cmdContext, RenderFrameRecord& frameRecord);
		void LightingPass(CommandContext cmdContext, RenderFrameRecord& frameRecord);

	private:
		bool SetupDeferredRenderPass();
		bool SetupLightingPass();
		bool SetupShadowMapPass();




		Renderer& Renderer;
		//GraphicsPipelineState<hlsl::StaticMeshResources> StaticMeshPipelineState{};

		DXTexture DepthBuffer{};
		DXTexture AlbedoBuffer{};
		DXTexture NormalBuffer{};

		DescriptorAllocation DepthBufferDSV{};
		DescriptorAllocation AlbedoBufferRTV{};
		DescriptorAllocation NormalBufferRTV{};
		DescriptorAllocation GBuffersSRV{};

		//GraphicsPipelineState<hlsl::ShadowMapResources> ShadowMapPipelineState{};
		DXTexture ShadowMap{};
		DescriptorAllocation ShadowMapDSV{};
		DescriptorAllocation ShadowMapSRV{};
		DescriptorAllocation ShadowMapSampler{};

		DXBuffer LightBuffer{};
		DescriptorAllocation LightBufferCBV{};
		DXBuffer LightTransformationMatricesBuffer{};
		DescriptorAllocation LightTransformationMatricesBufferCBV{};

		GraphicsPipelineState<hlsl::LightingResources> LightingPipelineState{};
		DXTexture OutputBuffer{};
		DescriptorAllocation OutputBufferRTV{};
		DescriptorAllocation OutputBufferSRV{};

		D3D12_VIEWPORT ShadowMapViewport{};
		D3D12_VIEWPORT Viewport{};
		D3D12_RECT ScissorRect{};
};

}