#include "DeferredRenderingPipeline.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"

#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

namespace rad
{

bool DeferredRenderingPipeline::Setup()
{
	ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	SetupShadowMapPass();
	SetupLightingPass();
	return true;
}

bool DeferredRenderingPipeline::OnResize(uint32_t width, uint32_t height)
{
	Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	DXTexture::TextureCreateInfo depthBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f}}
	};
	DepthBuffer = DXTexture::Create(Renderer.GetDevice(), L"DepthBuffer", depthBufferInfo, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	DXTexture::TextureCreateInfo albedoBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0, 0, 0, 1}}
	};
	AlbedoBuffer = DXTexture::Create(Renderer.GetDevice(), L"AlbedoBuffer", albedoBufferInfo);
	DXTexture::TextureCreateInfo normalBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .Color = {0, 0, 0, 1}}
	};
	NormalBuffer = DXTexture::Create(Renderer.GetDevice(), L"NormalBuffer", normalBufferInfo);
	DXTexture::TextureCreateInfo outputBufferInfo
	{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0, 0, 0, 1}}
	};
	OutputBuffer = DXTexture::Create(Renderer.GetDevice(), L"OutputBuffer", outputBufferInfo);

	// Create RTVs and DSVs
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	DepthBuffer.CreatePlacedDSV(DepthBufferDSV.GetView(), &dsvDesc);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	AlbedoBuffer.CreatePlacedRTV(AlbedoBufferRTV.GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalBuffer.CreatePlacedRTV(NormalBufferRTV.GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	OutputBuffer.CreatePlacedRTV(OutputBufferRTV.GetView(), &rtvDesc);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	OutputBuffer.CreatePlacedSRV(OutputBufferSRV.GetView(), &srvDesc);
	AlbedoBuffer.CreatePlacedSRV(GBuffersSRV.GetView(), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalBuffer.CreatePlacedSRV(GBuffersSRV.GetView(1), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DepthBuffer.CreatePlacedSRV(GBuffersSRV.GetView(2), &srvDesc);
	return true;
}

bool DeferredRenderingPipeline::SetupShadowMapPass()
{
	ShadowMap = DXTexture::Create(Renderer.GetDevice(), L"ShadowMap", {
		.Width = 1024,
		.Height = 1024,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f}}
		});
	ShadowMapViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, ShadowMap.Info.Width, ShadowMap.Info.Height);
	ShadowMapDSV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
	ShadowMapSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	ShadowMap.CreatePlacedDSV(ShadowMapDSV.GetView(), &dsvDesc);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	ShadowMap.CreatePlacedSRV(ShadowMapSRV.GetView(), &srvDesc);
	ShadowMapSampler = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1);

	//D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
	//	D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	//	D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	//	D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	//	FLOAT mipLODBias = 0,
	//	UINT maxAnisotropy = 16,
	//	D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
	//	D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
	//	FLOAT minLOD = 0.f,
	//	FLOAT maxLOD = D3D12_FLOAT32_MAX,
	//	D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
	//	UINT registerSpace = 0

	D3D12_SAMPLER_DESC shadowSampler{};
	shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	shadowSampler.MipLODBias = 0;
	shadowSampler.MaxAnisotropy = 16;
	shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	memset(shadowSampler.BorderColor, std::bit_cast<int>(1.0f), sizeof(shadowSampler.BorderColor));
	
	Renderer.GetDevice().CreateSampler(&shadowSampler, ShadowMapSampler.GetCPUHandle());
	
	Renderer.ViewableTextures.emplace("ShadowMap", std::pair<Ref<DXTexture>, DescriptorAllocationView>(Ref<DXTexture>(ShadowMap), ShadowMapSRV.GetView()));

	return true;
}

bool DeferredRenderingPipeline::SetupLightingPass()
{

	struct LightingPipelineStateStream : PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	auto [vertexShader, pixelShader] = Renderer.ShaderManager->CompileBindlessGraphicsShader(L"Lighting", RAD_SHADERS_DIR L"Graphics/Lighting.hlsl");
	
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateStream.RTVFormats = rtvFormats;

	pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	LightingPipelineState = PipelineState::Create("LightingPipeline", Renderer.GetDevice(), pipelineStateStream, &Renderer.ShaderManager->BindlessRootSignature);

	LightBuffer = DXBuffer::Create(Renderer.GetDevice(), L"LightBuffer", sizeof(rad::hlsl::LightDataBuffer), D3D12_HEAP_TYPE_DEFAULT);
	LightBufferCBV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	LightBuffer.CreatePlacedCBV(LightBufferCBV.GetView());


	LightTransformationMatricesBuffer = DXBuffer::Create(Renderer.GetDevice(), L"LightTransformationMatricesBuffer", sizeof(rad::hlsl::LightTransformBuffer), D3D12_HEAP_TYPE_DEFAULT);
	LightTransformationMatricesBufferCBV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	LightTransformationMatricesBuffer.CreatePlacedCBV(LightTransformationMatricesBufferCBV.GetView());

	DepthBufferDSV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
	AlbedoBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	NormalBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	OutputBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	
	GBuffersSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3);

	OutputBufferSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	
	Renderer.ViewableTextures.emplace("Albedo", std::pair<Ref<DXTexture>, DescriptorAllocationView>{ AlbedoBuffer, GBuffersSRV.GetView() });
	Renderer.ViewableTextures.emplace("Normal", std::pair<Ref<DXTexture>, DescriptorAllocationView>{ NormalBuffer, GBuffersSRV.GetView(1) });
	Renderer.ViewableTextures.emplace("Depth", std::pair<Ref<DXTexture>, DescriptorAllocationView>{ DepthBuffer, GBuffersSRV.GetView(2) });
	Renderer.ViewableTextures.emplace("Output", std::pair<Ref<DXTexture>, DescriptorAllocationView>{ OutputBuffer, OutputBufferSRV.GetView() });

	return true;
}
void DeferredRenderingPipeline::ShadowMapPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord)
{
	cmdContext->RSSetViewports(1, &ShadowMapViewport);
	cmdContext->RSSetScissorRects(1, &ScissorRect);

	TransitionVec(ShadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE).Execute(cmdContext);

	// Clear Render Targets
	{
		auto dsv = ShadowMapDSV.GetCPUHandle();
		cmdContext->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	// Set Render Targets
	{
		auto dsv = ShadowMapDSV.GetCPUHandle();
		cmdContext->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	}

	DepthOnlyPassData passData{ .CmdContext = cmdContext, .OutDepth = &ShadowMap };
	RenderView lightView = frameRecord.LightInfo.View;

	for (auto& renderCommand : frameRecord.Commands)
		renderCommand.DepthOnlyPass(lightView, passData);
}
void DeferredRenderingPipeline::DeferredRenderPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord)
{
	cmdContext->RSSetViewports(1, &Viewport);
	cmdContext->RSSetScissorRects(1, &ScissorRect);

	TransitionVec(AlbedoBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Add(NormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Add(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE)
		.Execute(cmdContext);

	// Clear Render Targets
	{
		auto rtv = AlbedoBufferRTV.GetCPUHandle();
		auto rtv2 = NormalBufferRTV.GetCPUHandle();
		auto dsv = DepthBufferDSV.GetCPUHandle();
		float clearColor[] = { 0, 0, 0, 1 };
		cmdContext->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmdContext->ClearRenderTargetView(rtv2, clearColor, 0, nullptr);
		cmdContext->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	// Set Render Targets
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv[2] = { AlbedoBufferRTV.GetCPUHandle(), NormalBufferRTV.GetCPUHandle() };
		auto dsv = DepthBufferDSV.GetCPUHandle();
		cmdContext->OMSetRenderTargets(2, rtv, FALSE, &dsv);
	}

	cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeferredPassData passData{ .CmdContext = cmdContext, .OutAlbedo = &AlbedoBuffer, .OutNormal = &NormalBuffer, .OutDepth = &DepthBuffer};
	for (auto& renderCommand : frameRecord.Commands)
		renderCommand.DeferredPass(frameRecord.View, passData);
}
void DeferredRenderingPipeline::LightingPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord)
{
	cmdContext->RSSetViewports(1, &Viewport);
	cmdContext->RSSetScissorRects(1, &ScissorRect);

	TransitionVec{}.Add(AlbedoBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(DepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(NormalBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(ShadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(OutputBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Execute(cmdContext);

	// Set Render Targets
	{
		auto rtv = OutputBufferRTV.GetCPUHandle();
		cmdContext->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Update Light Data
	{
		TransitionVec(LightBuffer, D3D12_RESOURCE_STATE_COPY_DEST).Execute(cmdContext);
		// Update Light Buffer
		hlsl::LightDataBuffer lightData{ .DirectionOrPosition = frameRecord.LightInfo.View.ViewDirection, .Color = frameRecord.LightInfo.Color, .Intensity = frameRecord.LightInfo.Intensity, .AmbientColor = frameRecord.LightInfo.AmbientColor };
		constexpr size_t paramCount = sizeof(rad::hlsl::LightDataBuffer) / sizeof(UINT);
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER params[paramCount] = {};
		for (size_t i = 0; i < paramCount; i++)
		{
			params[i].Dest = LightBuffer.GPUAddress(i * sizeof(UINT));
			params[i].Value = reinterpret_cast<const UINT*>(&lightData)[i];
		}
		cmdContext->WriteBufferImmediate(paramCount, params, nullptr);
		TransitionVec(LightBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER).Execute(cmdContext);
	}

	// Update Light Transformation Matrices
	{
		TransitionVec(LightTransformationMatricesBuffer, D3D12_RESOURCE_STATE_COPY_DEST).Execute(cmdContext);
		rad::hlsl::LightTransformBuffer lighTransform{};
		lighTransform.LightViewProjection = frameRecord.LightInfo.View.ViewProjectionMatrix;
		lighTransform.CamInverseView = glm::inverse(frameRecord.View.ViewMatrix);
		lighTransform.CamInverseProjection = glm::inverse(frameRecord.View.ProjectionMatrix);

		// Update Light Buffer
		constexpr size_t paramCount = sizeof(lighTransform) / sizeof(UINT);
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER params[paramCount] = {};
		for (size_t i = 0; i < paramCount; i++)
		{
			params[i].Dest = LightTransformationMatricesBuffer.GPUAddress(i * sizeof(UINT));
			params[i].Value = reinterpret_cast<const UINT*>(&lighTransform)[i];
		}
		cmdContext->WriteBufferImmediate(paramCount, params, nullptr);
		TransitionVec(LightTransformationMatricesBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER).Execute(cmdContext);
	}
	rad::hlsl::LightingResources lightingResources{};
	lightingResources.AlbedoTextureIndex = GBuffersSRV.Index;
	lightingResources.NormalTextureIndex = GBuffersSRV.GetView(1).GetIndex();
	lightingResources.DepthTextureIndex = GBuffersSRV.GetView(2).GetIndex();
	lightingResources.ShadowMapTextureIndex = ShadowMapSRV.Index;
	lightingResources.ShadowMapSamplerIndex = ShadowMapSampler.Index;
	lightingResources.LightDataBufferIndex = LightBufferCBV.Index;
	lightingResources.LightTransformBufferIndex = LightTransformationMatricesBufferCBV.Index;

	LightingPipelineState.BindWithResources(cmdContext, lightingResources);
	cmdContext->DrawInstanced(4, 1, 0, 0);
}
}