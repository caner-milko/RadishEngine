#pragma once

#include "RootSignature.h"
#include "PipelineState.h"

namespace dxpg
{

struct GenerateMipsPipeline
{
	bool Setup(ID3D12Device2* dev);

	void GenerateMips(ID3D12GraphicsCommandList4* cmdList, ID3D12Resource* src, ID3D12Resource* dest, int width, int height, int mipLevels);

	dx12::RootSignature RootSignature;
	dx12::PipelineState PipelineState;
};

}