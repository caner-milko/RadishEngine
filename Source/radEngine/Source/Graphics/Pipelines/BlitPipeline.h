#pragma once

#include "EngineCommon.h"

#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RendererCommon.h"
#include "Graphics/RenderGraph.h"

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
	void Blit(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& dstTex, RGBOutputResource& srcTex);

	Renderer& Renderer;
	GraphicsPipelineState<hlsl::BlitResources> PipelineState;
};

} // namespace rad