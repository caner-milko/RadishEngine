#pragma once

#include "EngineCommon.h"

#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RendererCommon.h"
#include "Graphics/DXResource.h"
#include "Graphics/ResourcePool.h"

namespace rad
{

#define SPD_MAX_MIP_LEVELS 12
struct GenerateMipsPipeline
{
	struct GlobalCounterStruct
	{
		uint32_t counters[6];
	};
	GenerateMipsPipeline(Renderer& renderer) : Renderer(renderer) {}
	bool Setup();

	void GenerateMips(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& tex);

	Renderer& Renderer;
	RootSignature RootSignature;
	PipelineState PipelineState;

	ResourcePool::OwnedResource* GlobalCounterBuffer; // GlobalCounterStruct
};

} // namespace rad