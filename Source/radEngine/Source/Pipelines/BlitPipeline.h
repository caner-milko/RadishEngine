#pragma once

#include "RadishCommon.h"

#include "RootSignature.h"
#include "PipelineState.h"

#include "RendererCommon.h"

namespace rad
{

struct BlitPipeline
{
	bool Setup(ID3D12Device2* dev);
	void Blit(ID3D12GraphicsCommandList2* cmdList, struct DXTexture* dstTex, struct DXTexture* srcTex, D3D12_CPU_DESCRIPTOR_HANDLE dstRTV, D3D12_GPU_DESCRIPTOR_HANDLE srcSRV);
	
	ID3D12Device2* Device = nullptr;
	RootSignature RootSignature;
	PipelineState PipelineState;
};

}