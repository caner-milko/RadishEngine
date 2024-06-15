#include "PipelineState.h"

#include "DXPGCommon.h"

namespace dxpg
{

PipelineState PipelineState::Create(std::string_view name, ID3D12Device2* device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, dxpg::RootSignature* rs)
{
	PipelineState ps{};
	ps.Name = name;
	ps.RootSignature = rs;

	ThrowIfFailed(device->CreatePipelineState(&pssd, IID_PPV_ARGS(&ps.DXPipelineState)));

	ps.DXPipelineState->SetName(s2ws(ps.Name).c_str());

	return ps;
}

void PipelineState::Bind(ID3D12GraphicsCommandList* cmdList) const
{
	cmdList->SetPipelineState(DXPipelineState.Get());
	cmdList->SetGraphicsRootSignature(RootSignature->DXSignature.Get());
}

}