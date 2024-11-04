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
	static PipelineState Create(std::string_view name, ID3D12Device2* device, T& pipelineStateStream, RootSignature* rs, bool compute)
	{
		pipelineStateStream.RS = rs->DXSignature.Get();
		D3D12_PIPELINE_STATE_STREAM_DESC pssd = {};
		pssd.SizeInBytes = sizeof(T);
		pssd.pPipelineStateSubobjectStream = &pipelineStateStream;

		return Create(name, device, pssd, rs, compute);
	}
	
	static PipelineState Create(std::string_view name, ID3D12Device2* device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, RootSignature* rs, bool compute);
	static PipelineState CreateComputePipeline(std::string_view name, ID3D12Device2* device, std::wstring_view shaderPath, RootSignature* rs, std::wstring_view entryPoint = L"CSMain", std::span<const std::wstring_view> includeFolders = {});
	
	void Bind(ID3D12GraphicsCommandList* cmdList) const;
	template<typename T>
	void ExecuteCompute(ID3D12GraphicsCommandList* cmdList, T const& resources, uint32_t x, uint32_t y, uint32_t z) const
	{
		Bind(cmdList);
		cmdList->SetComputeRoot32BitConstants(0, sizeof(T) / sizeof(uint32_t), &resources, 0);
		cmdList->Dispatch(x, y, z);
	}

	std::string Name;

	ComPtr<ID3D12PipelineState> DXPipelineState;
	RootSignature* RootSignature;
	bool IsCompute = false;
};
}