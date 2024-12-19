#pragma once

#include "RadishCommon.h"

#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RendererCommon.h"

namespace rad::hlsl
{
struct BlitResources;
}

namespace rad
{

struct BlitPipeline
{
	BlitPipeline(rad::Renderer& renderer) : Renderer(renderer) {}
	bool Setup();
	void Blit(CommandContext& commandCtx, struct DXTexture& dstTex, struct DXTexture& srcTex,
			  DescriptorAllocationView dstRTV, DescriptorAllocationView srcSRV);

	Renderer& Renderer;
	GraphicsPipelineState<hlsl::BlitResources> PipelineState;
};

} // namespace rad