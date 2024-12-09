#pragma once

#include "RendererCommon.h"
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
	static PipelineState Create(std::string_view name, RadDevice& device, T& pipelineStateStream, RootSignature* rs)
	{
		pipelineStateStream.RS = rs->DXSignature.Get();
		D3D12_PIPELINE_STATE_STREAM_DESC pssd = {};
		pssd.SizeInBytes = sizeof(T);
		pssd.pPipelineStateSubobjectStream = &pipelineStateStream;

		return Create(name, device, pssd, rs);
	}
	
	static PipelineState Create(std::string_view name, RadDevice& device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, RootSignature* rs);
	static PipelineState CreateComputePipeline(std::string_view name, Renderer& renderer, std::wstring_view shaderPath, RootSignature* rs, std::wstring_view entryPoint = L"CSMain", std::span<const std::wstring_view> includeFolders = {});

	std::string Name;

	ComPtr<ID3D12PipelineState> DXPipelineState;
	RootSignature* RootSignature;
};

template<typename T, bool IsCompute>
struct TypedPipelineState : PipelineState
{
	using PipelineState::PipelineState;
	TypedPipelineState(PipelineState&& ps) : PipelineState(std::move(ps)) {}
	TypedPipelineState& operator=(PipelineState&& ps)
	{
		PipelineState::operator=(std::move(ps));
		return *this;
	}
	inline void Bind(RadGraphicsCommandList& cmdList) const
	{
		cmdList.SetPipelineState(DXPipelineState.Get());
		if constexpr (!IsCompute)
			cmdList.SetGraphicsRootSignature(RootSignature->DXSignature.Get());
		else
			cmdList.SetComputeRootSignature(RootSignature->DXSignature.Get());
	}
	inline void SetResources(RadGraphicsCommandList& cmdList, T const& resources) const
	{
		if constexpr (!IsCompute)
			cmdList.SetGraphicsRoot32BitConstants(0, sizeof(T) / sizeof(uint32_t), &resources, 0);
		else
			cmdList.SetComputeRoot32BitConstants(0, sizeof(T) / sizeof(uint32_t), &resources, 0);
	}
	inline void BindWithResources(RadGraphicsCommandList& cmdList, T const& resources) const
	{
		Bind(cmdList);
		SetResources(cmdList, resources);
	}

	template<bool _ = false>
	requires (IsCompute)
	inline void ExecuteCompute(RadGraphicsCommandList& cmdList, T const& resources, uint32_t x, uint32_t y, uint32_t z) const
	{
		BindWithResources(cmdList, resources);
		cmdList.Dispatch(x, y, z);
	}
};

template<typename T>
using GraphicsPipelineState = TypedPipelineState<T, false>;
template<typename T>
using ComputePipelineState = TypedPipelineState<T, true>;

}