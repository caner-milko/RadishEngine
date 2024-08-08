#pragma once

#include "DXHelpers.h"

#include "RootSignature.h"

namespace rad
{

struct PipelineStateStreamBase
{
	CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RS;
};

struct PipelineState
{
	template<typename T>
	requires std::is_base_of_v<PipelineStateStreamBase, T>
	static PipelineState Create(std::string_view name, ID3D12Device2* device, T& pipelineStateStream, RootSignature* rs)
	{
		pipelineStateStream.RS = rs->DXSignature.Get();
		D3D12_PIPELINE_STATE_STREAM_DESC pssd = {};
		pssd.SizeInBytes = sizeof(T);
		pssd.pPipelineStateSubobjectStream = &pipelineStateStream;

		return Create(name, device, pssd, rs);
	}
	
	static PipelineState Create(std::string_view name, ID3D12Device2* device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, RootSignature* rs);

	void Bind(ID3D12GraphicsCommandList* cmdList) const;

	std::string Name;

	ComPtr<ID3D12PipelineState> DXPipelineState;
	RootSignature* RootSignature;
};
}