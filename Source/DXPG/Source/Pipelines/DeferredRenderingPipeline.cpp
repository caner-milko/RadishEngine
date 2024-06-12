#include "DeferredRenderingPipeline.h"

#include "ShaderManager.h"

namespace dxpg
{

struct ShaderMaterialInfo
{
	Vector4 Diffuse;
	int UseDiffuseTexture;
	int UseAlphaTexture;
};

struct ModelViewProjectionCB
{
	Matrix4x4 ModelViewProjection;
	Matrix4x4 NormalMatrix;
};

bool DeferredRenderingPipeline::Setup(ID3D12Device2* dev, uint32_t width, uint32_t height)
{
	Device = dev;
	// Create Static Mesh Pipeline
	{
		RootSignatureBuilder builder{};

		std::vector< CD3DX12_ROOT_PARAMETER1> rootParams;
		builder.AddConstants("ModelViewProjectionCB", sizeof(ModelViewProjectionCB) / 4, { .ShaderRegister = 0, .Visibility = D3D12_SHADER_VISIBILITY_VERTEX });
		builder.AddDescriptorTable("VertexSRV", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0) } }, D3D12_SHADER_VISIBILITY_VERTEX);
		builder.AddDescriptorTable("DiffuseTexture", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3) } }, D3D12_SHADER_VISIBILITY_PIXEL);
		builder.AddDescriptorTable("AlphaTexture", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4) } }, D3D12_SHADER_VISIBILITY_PIXEL);
		builder.AddConstants("MaterialCB", sizeof(ShaderMaterialInfo) / 4, { .ShaderRegister = 1, .Visibility = D3D12_SHADER_VISIBILITY_PIXEL });

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

		StaticMeshRootSignature = builder.Build("StaticMeshRS", dev, rootSignatureFlags);

		struct StaticMeshPipelineStateStream : PipelineStateStreamBase
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

		auto* vertexShader = ShaderManager::Get().CompileShader(L"Triangle.vs", DXPG_SHADERS_DIR L"Vertex/StaticMesh.vs.hlsl", ShaderType::Vertex);
		auto* pixelShader = ShaderManager::Get().CompileShader(L"Triangle.ps", DXPG_SHADERS_DIR L"Pixel/StaticMesh.ps.hlsl", ShaderType::Pixel);

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 2;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvFormats.RTFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		pipelineStateStream.RTVFormats = rtvFormats;

		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		StaticMeshPipelineState = PipelineState::Create("StaticMeshPipeline", dev, pipelineStateStream, &StaticMeshRootSignature);
	}

	// Create Lighting Pipeline
	{
		RootSignatureBuilder builder{};
		builder.AddDescriptorTable("GBuffers", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0) } }, D3D12_SHADER_VISIBILITY_PIXEL);
		builder.AddConstantBufferView("LightCB", { .ShaderRegister = 0, .Visibility = D3D12_SHADER_VISIBILITY_PIXEL, .DescFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE });

		CD3DX12_STATIC_SAMPLER_DESC staticSampler(0);
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSampler.MaxAnisotropy = 0;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		builder.AddStaticSampler(staticSampler);

		LightingRootSignature = builder.Build("LightingRS", dev, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		struct LightingPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} pipelineStateStream;

		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(ShaderManager::Get().CompileShader(L"Lighting.vs", DXPG_SHADERS_DIR L"Vertex/Lighting.vs.hlsl", ShaderType::Vertex)->Blob.Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ShaderManager::Get().CompileShader(L"Lighting.ps", DXPG_SHADERS_DIR L"Pixel/Lighting.ps.hlsl", ShaderType::Pixel)->Blob.Get());

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateStream.RTVFormats = rtvFormats;

		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		LightingPipelineState = PipelineState::Create("LightingPipeline", dev, pipelineStateStream, &LightingRootSignature);
	}

	LightBuffer = std::make_unique<DXBuffer>(DXBuffer::Create(dev, L"LightBuffer", sizeof(LightData), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));

	DepthBufferDSV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
	AlbedoBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	NormalBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	OutputBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	GBuffersSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);


	OnResize(width, height);

	return true;
}

void DeferredRenderingPipeline::OnResize(uint32_t width, uint32_t height)
{
	DXTexture::TextureCreateInfo depthBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f}}
	};
	DepthBuffer = std::make_unique<DXTexture>(DXTexture::Create(Device, L"DepthBuffer", depthBufferInfo, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	DXTexture::TextureCreateInfo albedoBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0, 0, 0, 1}}
	};
	AlbedoBuffer = std::make_unique<DXTexture>(DXTexture::Create(Device, L"AlbedoBuffer", albedoBufferInfo));
	DXTexture::TextureCreateInfo normalBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .Color = {0, 0, 0, 1}}
	};
	NormalBuffer = std::make_unique<DXTexture>(DXTexture::Create(Device, L"NormalBuffer", normalBufferInfo));
	DXTexture::TextureCreateInfo outputBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0, 0, 0, 1}}
	};
	OutputBuffer = std::make_unique<DXTexture>(DXTexture::Create(Device, L"OutputBuffer", outputBufferInfo));

	// Create RTVs and DSVs
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	DepthBuffer->CreatePlacedDSV(DepthBufferDSV->GetView(), &dsvDesc);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	AlbedoBuffer->CreatePlacedRTV(AlbedoBufferRTV->GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalBuffer->CreatePlacedRTV(NormalBufferRTV->GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	OutputBuffer->CreatePlacedRTV(OutputBufferRTV->GetView(), &rtvDesc);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	AlbedoBuffer->CreatePlacedSRV(GBuffersSRV->GetView(), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalBuffer->CreatePlacedSRV(GBuffersSRV->GetView(1), &srvDesc );
}

DXTexture& DeferredRenderingPipeline::Run(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx)
{
	RunStaticMeshPipeline(cmd, viewData, scene);
	RunLightingPipeline(cmd, viewData, scene, frameCtx);

	return *OutputBuffer;
}
void DeferredRenderingPipeline::RunStaticMeshPipeline(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene)
{
	// Transition Resources
	{
		D3D12_RESOURCE_BARRIER resBar[2] = {AlbedoBuffer->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET), NormalBuffer->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET) };
		cmd->ResourceBarrier(_countof(resBar), resBar);
		// Clear Render Targets
		auto rtv = AlbedoBufferRTV->GetCPUHandle();
		auto dsv = DepthBufferDSV->GetCPUHandle();
		auto rtv2 = NormalBufferRTV->GetCPUHandle();
		float clearColor[] = { 0, 0, 0, 1 };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->ClearRenderTargetView(rtv2, clearColor, 0, nullptr);
		//cmd->ClearRenderTargetView(OutputBufferRTV->GetCPUHandle(), clearColor, 0, nullptr);
		cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	// Set Render Targets
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv[2] = { AlbedoBufferRTV->GetCPUHandle(), NormalBufferRTV->GetCPUHandle() };
		auto dsv = DepthBufferDSV->GetCPUHandle();
		cmd->OMSetRenderTargets(2, rtv, FALSE, &dsv);
	}

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->SetPipelineState(StaticMeshPipelineState.DXPipelineState.Get());
	cmd->SetGraphicsRootSignature(StaticMeshPipelineState.RootSignature->DXSignature.Get());


	for (auto& group : scene.MeshGroups)
	{
		cmd->SetGraphicsRootDescriptorTable(StaticMeshPipelineState.RootSignature->NameToParameterIndices["VertexSRV"], group.VertexSRV);

		ComPtr<ID3D12DescriptorHeap> heap;
		for (int i = 0; i < group.Meshes.size(); i++)
		{
			auto& mesh = group.Meshes[i];
			cmd->IASetVertexBuffers(0, 1, &mesh.IndexBufferView);
			ModelViewProjectionCB mvp{};
			mvp.ModelViewProjection = XMMatrixMultiply(mesh.ModelMatrix, viewData.ViewProjection);
			mvp.NormalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, mesh.ModelMatrix));
			cmd->SetGraphicsRoot32BitConstants(StaticMeshPipelineState.RootSignature->NameToParameterIndices["ModelViewProjectionCB"], sizeof(ModelViewProjectionCB) / sizeof(uint32_t), &mvp, 0);

			ShaderMaterialInfo matInfo{};

			matInfo.UseDiffuseTexture = mesh.UseDiffuseTexture;
			matInfo.Diffuse = { mesh.DiffuseColor.x, mesh.DiffuseColor.y, mesh.DiffuseColor.z, 1 };

			if (mesh.UseDiffuseTexture)
				cmd->SetGraphicsRootDescriptorTable(StaticMeshPipelineState.RootSignature->NameToParameterIndices["DiffuseTexture"], mesh.DiffuseSRV);

			if (mesh.UseAlphaTexture)
				cmd->SetGraphicsRootDescriptorTable(StaticMeshPipelineState.RootSignature->NameToParameterIndices["AlphaTexture"], mesh.AlphaSRV);

			cmd->SetGraphicsRoot32BitConstants(StaticMeshPipelineState.RootSignature->NameToParameterIndices["MaterialCB"], sizeof(ShaderMaterialInfo) / sizeof(uint32_t), &matInfo, 0);

			cmd->DrawInstanced(mesh.IndexCount, 1, 0, 0);
		}
	}
}
void DeferredRenderingPipeline::RunLightingPipeline(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx)
{
	// Transition Resources
	{
		D3D12_RESOURCE_BARRIER resBar[3] = { AlbedoBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE), NormalBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE), OutputBuffer->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET) };
		cmd->ResourceBarrier(_countof(resBar), resBar);
	}

	// Set Render Targets
	{
		auto rtv = OutputBufferRTV->GetCPUHandle();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmd->SetPipelineState(LightingPipelineState.DXPipelineState.Get());
	cmd->SetGraphicsRootSignature(LightingPipelineState.RootSignature->DXSignature.Get());

	auto resBar = LightBuffer->Transition(D3D12_RESOURCE_STATE_COPY_DEST);
	cmd->ResourceBarrier(1, &resBar);
	// Update Light Buffer
	constexpr size_t paramCount = sizeof(LightData) / sizeof(UINT);
	D3D12_WRITEBUFFERIMMEDIATE_PARAMETER params[paramCount] = {};
	for (size_t i = 0; i < paramCount; i++)
	{
		params[i].Dest = LightBuffer->GPUAddress(i * sizeof(UINT));
		params[i].Value = reinterpret_cast<const UINT*>(&scene.Light)[i];
	}
	cmd->WriteBufferImmediate(paramCount, params, nullptr);
	resBar = LightBuffer->Transition(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	cmd->ResourceBarrier(1, &resBar);
	cmd->SetGraphicsRootConstantBufferView(LightingPipelineState.RootSignature->NameToParameterIndices["LightCB"], LightBuffer->GPUAddress());
	
	cmd->SetGraphicsRootDescriptorTable(LightingPipelineState.RootSignature->NameToParameterIndices["GBuffers"], GBuffersSRV->GetGPUHandle());

	cmd->DrawInstanced(4, 1, 0, 0);

	// Transition out to copy src
	{
		D3D12_RESOURCE_BARRIER resBar = OutputBuffer->Transition(D3D12_RESOURCE_STATE_COPY_SOURCE);
		cmd->ResourceBarrier(1, &resBar);
	}
}
}