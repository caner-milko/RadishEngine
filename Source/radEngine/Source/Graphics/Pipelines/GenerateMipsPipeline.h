#pragma once

#include "RadishCommon.h"

#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RendererCommon.h"
#include "Graphics/DXResource.h"

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

	void GenerateMips(CommandContext& commandCtx, struct DXTexture& texture);

	Renderer& Renderer;
	RootSignature RootSignature;
	PipelineState PipelineState;

	DXTypedSingularBuffer<GlobalCounterStruct> GlobalCounterBuffer;
	DescriptorAllocation GlobalCounterUAV;
};

} // namespace rad