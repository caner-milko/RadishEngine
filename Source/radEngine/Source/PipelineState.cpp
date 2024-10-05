#include "PipelineState.h"

#include "RadishCommon.h"
#include "ShaderManager.h"
namespace rad
{

PipelineState PipelineState::Create(std::string_view name, ID3D12Device2* device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, rad::RootSignature* rs, bool compute)
{
	PipelineState ps{};
	ps.Name = name;
	ps.RootSignature = rs;

	ThrowIfFailed(device->CreatePipelineState(&pssd, IID_PPV_ARGS(&ps.DXPipelineState)));

	ps.DXPipelineState->SetName(s2ws(ps.Name).c_str());
	ps.IsCompute = compute;
	return ps;
}

PipelineState PipelineState::CreateComputePipeline(std::string_view name, ID3D12Device2* device, std::wstring_view shaderPath, rad::RootSignature* rs, std::wstring_view entryPoint, std::span<const std::wstring_view> includeFolders)
{
	struct ComputePipelineStream : PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream{};
	std::wstring shaderName = s2ws(name) + L"_Shader";
	auto cs = ShaderManager::Get().CompileBindlessComputeShader(shaderName, shaderPath, entryPoint, includeFolders);
	pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(cs->Blob.Get());
	return Create(name, device, pipelineStateStream, rs, true);
}

void PipelineState::Bind(ID3D12GraphicsCommandList* cmdList) const
{
	cmdList->SetPipelineState(DXPipelineState.Get());
	if (!IsCompute)
		cmdList->SetGraphicsRootSignature(RootSignature->DXSignature.Get());
	else
		cmdList->SetComputeRootSignature(RootSignature->DXSignature.Get());
}

}