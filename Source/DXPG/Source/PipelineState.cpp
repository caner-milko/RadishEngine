#include "PipelineState.h"

#include "DXPGCommon.h"

namespace dxpg::dx12
{

PipelineState PipelineState::Create(std::string_view name, ID3D12Device2* device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, dx12::RootSignature* rs)
{
	PipelineState ps{};
	ps.Name = name;
	ps.RootSignature = rs;

	ThrowIfFailed(device->CreatePipelineState(&pssd, IID_PPV_ARGS(&ps.DXPipelineState)));

	ps.DXPipelineState->SetName(s2ws(ps.Name).c_str());

	return ps;
}

}