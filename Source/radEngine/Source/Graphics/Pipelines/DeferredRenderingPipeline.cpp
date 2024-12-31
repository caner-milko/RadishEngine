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
	SetupScreenSpaceRaymarchPass();
	return true;
}

bool DeferredRenderingPipeline::OnResize(uint32_t width, uint32_t height)
{
	Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	DXTexture::TextureCreateInfo depthBufferInfo{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f}}};
	DepthBuffer =
		DXTexture::Create(Renderer.GetDevice(), L"DepthBuffer", depthBufferInfo, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	SSDepthBuffer =
		DXTexture::Create(Renderer.GetDevice(), L"SSDepthBuffer", depthBufferInfo, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	DXTexture::TextureCreateInfo albedoBufferInfo{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0, 0, 0, 1}}};
	AlbedoBuffer = DXTexture::Create(Renderer.GetDevice(), L"AlbedoBuffer", albedoBufferInfo);
	DXTexture::TextureCreateInfo normalBufferInfo{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .Color = {0, 0, 0, 1}}};
	NormalBuffer = DXTexture::Create(Renderer.GetDevice(), L"NormalBuffer", normalBufferInfo);

	auto reflectionBufferInfo = albedoBufferInfo;
	reflectionBufferInfo.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	reflectionBufferInfo.ClearValue =
		D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .Color = {0, 0, 0, 0}};

	SSReflectRefractBuffer =
		DXTexture::Create(Renderer.GetDevice(), L"SSReflectRefractBuffer", reflectionBufferInfo);

	reflectionBufferInfo.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ReflectionResultBuffer =
		DXTexture::Create(Renderer.GetDevice(), L"ReflectionResultBuffer", reflectionBufferInfo);
	RefractionResultBuffer =
		DXTexture::Create(Renderer.GetDevice(), L"RefractionResultBuffer", reflectionBufferInfo);

	DXTexture::TextureCreateInfo outputBufferInfo{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0, 0, 0, 1}}};
	OutputBuffer = DXTexture::Create(Renderer.GetDevice(), L"OutputBuffer", outputBufferInfo);
	LightingResultBuffer = DXTexture::Create(Renderer.GetDevice(), L"LightingResultBuffer", outputBufferInfo);

	// Create RTVs and DSVs
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	DepthBuffer.CreatePlacedDSV(DepthBufferDSV.GetView(), &dsvDesc);
	SSDepthBuffer.CreatePlacedDSV(SSDepthBufferDSV.GetView(), &dsvDesc);


	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	AlbedoBuffer.CreatePlacedRTV(AlbedoBufferRTV.GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalBuffer.CreatePlacedRTV(NormalBufferRTV.GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	OutputBuffer.CreatePlacedRTV(OutputBufferRTV.GetView(), &rtvDesc);
	LightingResultBuffer.CreatePlacedRTV(LightingResultBufferRTV.GetView(), &rtvDesc);

	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	SSReflectRefractBuffer.CreatePlacedRTV(SSReflectRefractBufferRTV.GetView(), &rtvDesc);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	OutputBuffer.CreatePlacedSRV(OutputBufferSRV.GetView(), &srvDesc);
	LightingResultBuffer.CreatePlacedSRV(LightingResultBufferSRV.GetView(), &srvDesc);
	AlbedoBuffer.CreatePlacedSRV(GBuffersSRV.GetView(), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalBuffer.CreatePlacedSRV(GBuffersSRV.GetView(1), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DepthBuffer.CreatePlacedSRV(GBuffersSRV.GetView(2), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	SSReflectRefractBuffer.CreatePlacedSRV(GBuffersSRV.GetView(3), &srvDesc);
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SSDepthBuffer.CreatePlacedSRV(GBuffersSRV.GetView(4), &srvDesc);

	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	ReflectionResultBuffer.CreatePlacedSRV(ReflectionResultBufferSRV.GetView(), &srvDesc);
	RefractionResultBuffer.CreatePlacedSRV(RefractionResultBufferSRV.GetView(), &srvDesc);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	ReflectionResultBuffer.CreatePlacedUAV(ReflectionResultBufferUAV.GetView(), &uavDesc);
	RefractionResultBuffer.CreatePlacedUAV(RefractionResultBufferUAV.GetView(), &uavDesc);
		
	return true;
}

bool DeferredRenderingPipeline::SetupShadowMapPass()
{
	ShadowMap = DXTexture::Create(
		Renderer.GetDevice(), L"ShadowMap",
		{.Width = 1024,
		 .Height = 1024,
		 .MipLevels = 1,
		 .Format = DXGI_FORMAT_D32_FLOAT,
		 .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		 .ClearValue = D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f}}});
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

	// D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
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

	Renderer.ViewableTextures.emplace("ShadowMap", std::pair<Ref<DXTexture>, DescriptorAllocationView>(
													   Ref<DXTexture>(ShadowMap), ShadowMapSRV.GetView()));

	return true;
}

bool DeferredRenderingPipeline::SetupScreenSpaceRaymarchPass()
{
	ScreenSpaceRaymarchPipelineState = PipelineState::CreateBindlessComputePipeline(
		"LightingPipeline", Renderer, RAD_SHADERS_DIR L"Compute/ScreenSpaceRaymarch.cs.hlsl");

	ReflectionResultBufferSRV =
		g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	ReflectionResultBufferUAV =
		g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	RefractionResultBufferSRV =
		g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	RefractionResultBufferUAV =
		g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	Renderer.ViewableTextures.emplace("ReflectionResult", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
																		ReflectionResultBuffer,
																		ReflectionResultBufferSRV.GetView()});
	Renderer.ViewableTextures.emplace("RefractionResult", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
																		RefractionResultBuffer,
																		RefractionResultBufferSRV.GetView()});
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

	auto [vertexShader, pixelShader] =
		Renderer.ShaderManager->CompileBindlessGraphicsShader(L"Lighting", RAD_SHADERS_DIR L"Graphics/Lighting.hlsl");

	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateStream.RTVFormats = rtvFormats;

	pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	LightingPipelineState = PipelineState::Create("LightingPipeline", Renderer.GetDevice(), pipelineStateStream,
												  &Renderer.ShaderManager->BindlessRootSignature);

	LightBuffer = DXBuffer::Create(Renderer.GetDevice(), L"LightBuffer", sizeof(rad::hlsl::LightDataBuffer),
								   D3D12_HEAP_TYPE_DEFAULT);
	LightBufferCBV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	LightBuffer.CreatePlacedCBV(LightBufferCBV.GetView());

	ViewTransformBuffer =
		DXTypedSingularBuffer<hlsl::ViewTransformBuffer>::Create(Renderer.GetDevice(), L"LightTransformationMatricesBuffer");
	ViewTransformBufferCBV =
		g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	ViewTransformBuffer.CreatePlacedCBV(ViewTransformBufferCBV.GetView());

	DepthBufferDSV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
	SSDepthBufferDSV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
	AlbedoBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	NormalBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	OutputBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	LightingResultBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	SSReflectRefractBufferRTV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);

	GBuffersSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5);

	OutputBufferSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	LightingResultBufferSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

	Renderer.ViewableTextures.emplace(
		"Albedo", std::pair<Ref<DXTexture>, DescriptorAllocationView>{AlbedoBuffer, GBuffersSRV.GetView()});
	Renderer.ViewableTextures.emplace(
		"Normal", std::pair<Ref<DXTexture>, DescriptorAllocationView>{NormalBuffer, GBuffersSRV.GetView(1)});
	Renderer.ViewableTextures.emplace(
		"Depth", std::pair<Ref<DXTexture>, DescriptorAllocationView>{DepthBuffer, GBuffersSRV.GetView(2)});
	Renderer.ViewableTextures.emplace(
		"Output", std::pair<Ref<DXTexture>, DescriptorAllocationView>{OutputBuffer, OutputBufferSRV.GetView()});
	Renderer.ViewableTextures.emplace("LightingResult", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
															LightingResultBuffer, LightingResultBufferSRV.GetView()});
	Renderer.ViewableTextures.emplace("SSReflectRefract", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
														SSReflectRefractBuffer, GBuffersSRV.GetView(3)});
	Renderer.ViewableTextures.emplace("SSDepth", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
														SSDepthBuffer, GBuffersSRV.GetView(4)});
	return true;
}

void DeferredRenderingPipeline::BeginFrame(CommandContext& cmdContext, RenderFrameRecord& frameRecord) 
{

	// Update Light Data
	{
		TransitionVec(LightBuffer, D3D12_RESOURCE_STATE_COPY_DEST).Execute(cmdContext);
		// Update Light Buffer
		hlsl::LightDataBuffer lightData{.DirectionOrPosition = frameRecord.LightInfo.View.ViewDirection,
										.Color = frameRecord.LightInfo.Color,
										.Intensity = frameRecord.LightInfo.Intensity,
										.AmbientColor = frameRecord.LightInfo.AmbientColor};
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
		rad::hlsl::ViewTransformBuffer lightTransform{};
		lightTransform.CamView = frameRecord.View.ViewMatrix;
		lightTransform.CamProjection = frameRecord.View.ProjectionMatrix;
		lightTransform.CamViewProjection = frameRecord.View.ViewProjectionMatrix;
		lightTransform.CamInverseView = glm::inverse(frameRecord.View.ViewMatrix);
		lightTransform.CamInverseProjection = glm::inverse(frameRecord.View.ProjectionMatrix);
		lightTransform.CamInverseViewProjection = lightTransform.CamInverseView * lightTransform.CamInverseProjection;
		lightTransform.CamNear = frameRecord.View.NearPlane;
		lightTransform.CamFar = frameRecord.View.FarPlane;

		lightTransform.LightViewProjection = frameRecord.LightInfo.View.ViewProjectionMatrix;

		// Update Light Buffer
		ViewTransformBuffer.WriteImmediate(cmdContext, lightTransform);
		TransitionVec(ViewTransformBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
			.Execute(cmdContext);
	}
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

	DepthOnlyPassData passData{.CmdContext = cmdContext, .OutDepth = &ShadowMap};
	RenderView lightView = frameRecord.LightInfo.View;

	for (auto& renderCommand : frameRecord.Commands)
		if (renderCommand.DepthOnlyPass)
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
		float clearColor[] = {0, 0, 0, 1};
		cmdContext->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmdContext->ClearRenderTargetView(rtv2, clearColor, 0, nullptr);
		cmdContext->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	// Set Render Targets
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv[2] = {AlbedoBufferRTV.GetCPUHandle(), NormalBufferRTV.GetCPUHandle()};
		auto dsv = DepthBufferDSV.GetCPUHandle();
		cmdContext->OMSetRenderTargets(2, rtv, FALSE, &dsv);
	}

	cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeferredPassData passData{
		.CmdContext = cmdContext, .OutAlbedo = &AlbedoBuffer, .OutNormal = &NormalBuffer, .OutDepth = &DepthBuffer};
	for (auto& renderCommand : frameRecord.Commands)
		if (renderCommand.DeferredPass)
			renderCommand.DeferredPass(frameRecord.View, passData);
}
void DeferredRenderingPipeline::WaterRenderPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord)
{
	cmdContext->RSSetViewports(1, &Viewport);
	cmdContext->RSSetScissorRects(1, &ScissorRect);

	TransitionVec{}
		.Add(DepthBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE)
		.Add(SSDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST)
		.Execute(cmdContext);

	cmdContext->CopyResource(SSDepthBuffer.Resource.Get(), DepthBuffer.Resource.Get());

	TransitionVec{}
		.Add(SSReflectRefractBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Add(SSDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE)
		.Execute(cmdContext);

	// Clear Render Targets
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = SSReflectRefractBufferRTV.GetCPUHandle();
	float clearColor[] = {0, 0, 0, 0};
	cmdContext->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

	// Set Render Targets
	auto dsv = SSDepthBufferDSV.GetCPUHandle();
	cmdContext->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	WaterPassData passData{.CmdContext = cmdContext,
						   .OutReflectionRefraction = &SSReflectRefractBuffer,
						   .OutDepth = &SSDepthBuffer,
						   .InViewTransformCBV = ViewTransformBufferCBV.GetView()};
	for (auto& renderCommand : frameRecord.Commands)
		if (renderCommand.WaterPass)
			renderCommand.WaterPass(frameRecord.View, passData);
}

void DeferredRenderingPipeline::LightingPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord)
{
	cmdContext->RSSetViewports(1, &Viewport);
	cmdContext->RSSetScissorRects(1, &ScissorRect);

	TransitionVec{}
		.Add(AlbedoBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(DepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(NormalBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(ShadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(LightingResultBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Add(LightBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		.Add(ViewTransformBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		.Add(ReflectionResultBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(RefractionResultBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Execute(cmdContext);

	// Set Render Targets
	{
		auto rtv = LightingResultBufferRTV.GetCPUHandle();
		cmdContext->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	rad::hlsl::LightingResources lightingResources{};
	lightingResources.AlbedoTextureIndex = GBuffersSRV.Index;
	lightingResources.NormalTextureIndex = GBuffersSRV.GetView(1).GetIndex();
	lightingResources.DepthTextureIndex = GBuffersSRV.GetView(2).GetIndex();
	lightingResources.ShadowMapTextureIndex = ShadowMapSRV.Index;
	lightingResources.ShadowMapSamplerIndex = ShadowMapSampler.Index;
	lightingResources.LightDataBufferIndex = LightBufferCBV.Index;
	lightingResources.ViewTransformBufferIndex = ViewTransformBufferCBV.Index;
	lightingResources.ReflectionResultIndex = ReflectionResultBufferSRV.Index;
	lightingResources.RefractionResultIndex = RefractionResultBufferSRV.Index;

	LightingPipelineState.BindWithResources(cmdContext, lightingResources);
	cmdContext->DrawInstanced(4, 1, 0, 0);

	//Copy lighting result to output buffer
	TransitionVec{}
		.Add(LightingResultBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE)
		.Add(OutputBuffer, D3D12_RESOURCE_STATE_COPY_DEST)
		.Execute(cmdContext);

	cmdContext->CopyResource(OutputBuffer.Resource.Get(), LightingResultBuffer.Resource.Get());
}
void DeferredRenderingPipeline::ForwardRenderPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord)
{
	cmdContext->RSSetViewports(1, &Viewport);
	cmdContext->RSSetScissorRects(1, &ScissorRect);

	TransitionVec{}
		.Add(AlbedoBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(SSDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ)
		.Add(LightingResultBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(OutputBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Add(ReflectionResultBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(RefractionResultBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Execute(cmdContext);

	// Set Render Targets
	{
		auto rtv = OutputBufferRTV.GetCPUHandle();
		auto dsv = SSDepthBufferDSV.GetCPUHandle();
		cmdContext->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	}

	cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ForwardPassData passData{.CmdContext = cmdContext,
							 .OutColor = &OutputBuffer,
							 .Depth = &SSDepthBuffer,
							 .InViewTransformCBV = ViewTransformBufferCBV.GetView(),
							 .InColorSRV = LightingResultBufferSRV.GetView(),
							 .InReflectionResultSRV = ReflectionResultBufferSRV.GetView(),
							 .InRefractionResultSRV = RefractionResultBufferSRV.GetView()};
	for (auto& renderCommand : frameRecord.Commands)
		if (renderCommand.ForwardPass)
			renderCommand.ForwardPass(frameRecord.View, passData);
}
void DeferredRenderingPipeline::ScreenSpaceRaymarchPass(CommandContext& cmdContext, RenderFrameRecord& frameRecord) 
{
	TransitionVec{}
		.Add(SSReflectRefractBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(SSDepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(DepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		.Add(ReflectionResultBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.Add(RefractionResultBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.Add(ViewTransformBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		.Execute(cmdContext);
	hlsl::ScreenSpaceRaymarchResources resources{};

	resources.InReflectRefractNormalTextureIndex = GBuffersSRV.GetView(3).GetIndex();
	resources.SSDepthTextureIndex = GBuffersSRV.GetView(4).GetIndex();
	resources.DepthTextureIndex = GBuffersSRV.GetView(2).GetIndex();
	resources.OutReflectResultTextureIndex = ReflectionResultBufferUAV.Index;
	resources.OutRefractResultTextureIndex = RefractionResultBufferUAV.Index;
	resources.ViewTransformBufferIndex = ViewTransformBufferCBV.Index;

	uint32_t width = ReflectionResultBuffer.Info.Width;
	uint32_t height = RefractionResultBuffer.Info.Height;

	ScreenSpaceRaymarchPipelineState.ExecuteCompute(cmdContext, resources, (width + 7) / 8, (height + 7) / 8, 1);
}
} // namespace rad