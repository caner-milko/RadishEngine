#include "PipelineState.h"

#include "Renderer.h"
#include "RadishCommon.h"
#include "ShaderManager.h"
namespace rad
{

PipelineState PipelineState::Create(std::string_view name, RadDevice& device, D3D12_PIPELINE_STATE_STREAM_DESC const& pssd, rad::RootSignature* rs)
{
	PipelineState ps{};
	ps.Name = name;
	ps.RootSignature = rs;

	ThrowIfFailed(device.CreatePipelineState(&pssd, IID_PPV_ARGS(&ps.DXPipelineState)));

	ps.DXPipelineState->SetName(s2ws(ps.Name).c_str());
	return ps;
}

PipelineState PipelineState::CreateBindlessComputePipeline(std::string_view name, Renderer& renderer, std::wstring_view shaderPath, std::wstring_view entryPoint, std::span<const std::wstring_view> includeFolders)
{
	struct ComputePipelineStream : PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream{};
	std::wstring shaderName = s2ws(name) + L"_Shader";
	auto cs = renderer.ShaderManager->CompileBindlessComputeShader(shaderName, shaderPath, entryPoint, includeFolders);
	pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(cs->Blob.Get());
	return Create(name, renderer.GetDevice(), pipelineStateStream, &renderer.ShaderManager->BindlessRootSignature);
}

}