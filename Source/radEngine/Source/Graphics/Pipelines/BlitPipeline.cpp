#include "BlitPipeline.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Renderer.h"
#include "Graphics/RenderGraphHelpers.h"
namespace rad
{
bool BlitPipeline::Setup()
{

	struct BlitPipelineStream : PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	auto [vertexShader, pixelShader] =
		Renderer.ShaderManager->CompileBindlessGraphicsShader(L"Blit", RAD_ENGINE_SHADERS_DIR L"Graphics/Blit.hlsl");

	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateStream.RTVFormats = rtvFormats;

	pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	PipelineState = PipelineState::Create(
		"BlitPipeline", Renderer.GetDevice(), pipelineStateStream, &Renderer.ShaderManager->BindlessRootSignature);
	return true;
}

void BlitPipeline::Blit(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& dstTex, RGBOutputResource& srcTex)
{
	auto& pass = rgBuilder.AddPass("Blit");
	auto dstInput = pass.AddInResourceSetOut("DstInput", dstTex, RGResourceUsage::RenderTargetView(*dstTex));
	auto srcInput = pass.AddInput("SrcInput", srcTex, RGResourceUsage::PixelShaderResourceView(srcTex));
	pass.Execute = [this, dstInput = dstInput->GetResourceView(), srcInput = srcInput->GetResourceView()](
					   CommandContext& commandCtx) {
		auto& dstDesc = dstInput.GetCreateInfo().Desc;
		D3D12_RECT scissorRect = {};
		scissorRect.right = dstDesc.Width;
		scissorRect.bottom = dstDesc.Height;
		commandCtx->RSSetScissorRects(1, &scissorRect);
		D3D12_VIEWPORT viewport = {};
		viewport.Width = static_cast<float>(dstDesc.Width);
		viewport.Height = static_cast<float>(dstDesc.Height);
		commandCtx->RSSetViewports(1, &viewport);

		auto rtvHandle = dstInput.AsCPUDescriptor<RenderTargetViewDesc>().GetCPUHandle();
		commandCtx->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		commandCtx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		rad::hlsl::BlitResources blitResources{};
		blitResources.SourceTextureIndex = srcInput.AsGPUDescriptor<ShaderResourceViewDesc>().Index;
		PipelineState.BindWithResources(commandCtx, blitResources);
		commandCtx->DrawInstanced(4, 1, 0, 0);
	};
}

} // namespace rad
