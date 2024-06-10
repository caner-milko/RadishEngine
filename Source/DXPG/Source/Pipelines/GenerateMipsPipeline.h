#pragma once

#include "DXPGCommon.h"

#include "RootSignature.h"
#include "PipelineState.h"

#include "RendererCommon.h"

namespace dxpg
{

#define SPD_MAX_MIP_LEVELS 12
struct GenerateMipsPipeline
{
	bool Setup(ID3D12Device2* dev);

	void GenerateMips(FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, struct DXTexture& texture, uint32_t width, uint32_t height, uint32_t arraySize = 1);

	ID3D12Device2* Device = nullptr;
	RootSignature RootSignature;
	PipelineState PipelineState;

	std::unique_ptr<DXBuffer> GlobalCounterBuffer;
	std::unique_ptr<UnorderedAccessView> GlobalCounterUAV;
};

}