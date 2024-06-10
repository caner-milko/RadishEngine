#include "StaticMeshPipeline.h"

#include "ShaderManager.h"

namespace dxpg
{

struct ShaderMaterialInfo
{
	Vector4 Diffuse;
	int UseDiffuseTexture;
	int UseAlphaTexture;
};

bool StaticMeshPipeline::Setup(ID3D12Device2* dev)
{
	// Create Root Signature
	RootSignatureBuilder builder{};

	std::vector< CD3DX12_ROOT_PARAMETER1> rootParams;
	builder.AddConstants("ModelViewProjectionCB", sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	builder.AddDescriptorTable("VertexSRV", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0) } }, D3D12_SHADER_VISIBILITY_VERTEX);
	builder.AddDescriptorTable("DiffuseTexture", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3) } }, D3D12_SHADER_VISIBILITY_PIXEL);
	builder.AddDescriptorTable("AlphaTexture", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4) } }, D3D12_SHADER_VISIBILITY_PIXEL);
	builder.AddConstants("MaterialCB", sizeof(ShaderMaterialInfo) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);


	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_STATIC_SAMPLER_DESC staticSampler(0);
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.MaxAnisotropy = 16;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	builder.AddStaticSampler(staticSampler);

	RootSignature = builder.Build("StaticMeshRS", dev, rootSignatureFlags);

	struct PipelineStateStream : PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSINDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMALINDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDINDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	auto* vertexShader = ShaderManager::Get().CompileShader(L"Triangle.vs", DXPG_SHADERS_DIR L"Vertex/Triangle.vs.hlsl", ShaderType::Vertex);
	auto* pixelShader = ShaderManager::Get().CompileShader(L"Triangle.ps", DXPG_SHADERS_DIR L"Pixel/Triangle.ps.hlsl", ShaderType::Pixel);

	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

	pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateStream.RTVFormats = rtvFormats;

	pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	PipelineState = PipelineState::Create("StaticMeshPipeline", dev, pipelineStateStream, &RootSignature);
	return true;
}

bool StaticMeshPipeline::Run(ID3D12GraphicsCommandList* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx)
{
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->SetPipelineState(PipelineState.DXPipelineState.Get());
	cmd->SetGraphicsRootSignature(RootSignature.DXSignature.Get());
	for (auto& group : scene.MeshGroups)
	{
		cmd->SetGraphicsRootDescriptorTable(RootSignature.NameToParameterIndices["VertexSRV"], group.VertexSRV);
	
		ComPtr<ID3D12DescriptorHeap> heap;
		for (int i = 0; i < group.Meshes.size(); i++)
		{
			auto& mesh = group.Meshes[i];
			cmd->IASetVertexBuffers(0, 1, &mesh.IndexBufferView);
			DirectX::XMMATRIX mvpMatrix = XMMatrixMultiply(mesh.ModelMatrix, viewData.ViewProjection);
			cmd->SetGraphicsRoot32BitConstants(RootSignature.NameToParameterIndices["ModelViewProjectionCB"], sizeof(Matrix4x4) / sizeof(uint32_t), &mvpMatrix, 0);

			ShaderMaterialInfo matInfo{};

			matInfo.UseDiffuseTexture = mesh.UseDiffuseTexture;
			matInfo.Diffuse = { mesh.DiffuseColor.x, mesh.DiffuseColor.y, mesh.DiffuseColor.z, 1 };

			if (mesh.UseDiffuseTexture)
				cmd->SetGraphicsRootDescriptorTable(RootSignature.NameToParameterIndices["DiffuseTexture"], mesh.DiffuseSRV);

			if (mesh.UseAlphaTexture)
				cmd->SetGraphicsRootDescriptorTable(RootSignature.NameToParameterIndices["AlphaTexture"], mesh.AlphaSRV);

			cmd->SetGraphicsRoot32BitConstants(RootSignature.NameToParameterIndices["MaterialCB"], sizeof(ShaderMaterialInfo) / sizeof(uint32_t), &matInfo, 0);

			cmd->DrawInstanced(mesh.IndexCount, 1, 0, 0);
		}
	}
	return false;
}
}