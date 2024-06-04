#pragma once

#include "RootSignature.h"
#include "PipelineState.h"

#include "RendererCommon.h"

namespace dxpg
{

struct StaticMeshPipeline
{
	bool Setup(ID3D12Device2* dev);
	
	bool Run(ID3D12GraphicsCommandList* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx);

	dx12::RootSignature RootSignature;
	dx12::PipelineState PipelineState;
};

}