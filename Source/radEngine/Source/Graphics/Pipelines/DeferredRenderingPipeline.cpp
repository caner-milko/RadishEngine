#include "DeferredRenderingPipeline.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"
#include <Graphics/RenderGraphHelpers.h>

#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

namespace rad
{

bool DeferredRenderingPipeline::Setup()
{
	ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	{
		ScreenSpaceRaymarchPipelineState = PipelineState::CreateBindlessComputePipeline(
			"LightingPipeline", Renderer, RAD_ENGINE_SHADERS_DIR L"Compute/ScreenSpaceRaymarch.cs.hlsl");
	}
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

		auto [vertexShader, pixelShader] = Renderer.ShaderManager->CompileBindlessGraphicsShader(
			L"Lighting", RAD_ENGINE_SHADERS_DIR L"Graphics/Lighting.hlsl");

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateStream.RTVFormats = rtvFormats;

		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		LightingPipelineState = PipelineState::Create("LightingPipeline",
													  Renderer.GetDevice(),
													  pipelineStateStream,
													  &Renderer.ShaderManager->BindlessRootSignature);
	}

	{
		ShadowMapSampler = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1);
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
	}

	return true;
}

bool DeferredRenderingPipeline::OnResize(uint32_t width, uint32_t height)
{
	Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	return true;
}

void DeferredRenderingPipeline::BuildFrameRenderGraph(RenderGraphBuilder& graphBuilder, SceneRenderData& sceneData)
{
	// BeginFrame(graphBuilder, sceneData);
	/*
	1. Create Light and View Buffers
	2. Create required textures(Depth, SSDepth, Albedo, Normal, Output, SSReflectRefract, ReflectionResult,
	RefractionResult, LightingResult, ShadowMap)
	*/

	auto depthBufCreateInfo = rad::ResourceCreateHelper::Texture2D(
		sceneData.View.Width, sceneData.View.Height, DXGI_FORMAT_D32_FLOAT, ResourcePresetFlags::DepthStencil);

	auto depthBuf = graphBuilder.AddGraphResource("DepthBuffer", depthBufCreateInfo);

	auto ssDepth = graphBuilder.AddGraphResource("SSDepthBuffer", depthBufCreateInfo);

	auto albedoBuf = graphBuilder.AddGraphResource(
		"AlbedoBuffer",
		rad::ResourceCreateHelper::Texture2D(sceneData.View.Width,
											 sceneData.View.Height,
											 DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
											 ResourcePresetFlags::RenderTarget,
											 ResourceCreateHelper::TextureDetails{.ClearValue = {0, 0, 0, 1}}));
	auto normalBuf = graphBuilder.AddGraphResource(
		"NormalBuffer",
		rad::ResourceCreateHelper::Texture2D(sceneData.View.Width,
											 sceneData.View.Height,
											 DXGI_FORMAT_R16G16B16A16_FLOAT,
											 ResourcePresetFlags::RenderTarget,
											 ResourceCreateHelper::TextureDetails{.ClearValue = {0, 0, 0, 1}}));

	auto reflectRefractBuf = graphBuilder.AddGraphResource(
		"SSReflectRefractBuffer",
		rad::ResourceCreateHelper::Texture2D(sceneData.View.Width,
											 sceneData.View.Height,
											 DXGI_FORMAT_R16G16B16A16_FLOAT,
											 ResourcePresetFlags::RenderTarget,
											 ResourceCreateHelper::TextureDetails{.ClearValue = {0, 0, 0, 0}}));

	auto reflectionResultBufferInfo =
		rad::ResourceCreateHelper::Texture2D(sceneData.View.Width,
											 sceneData.View.Height,
											 DXGI_FORMAT_R16G16B16A16_FLOAT,
											 ResourcePresetFlags::RenderTarget | ResourcePresetFlags::UnorderedAccess,
											 ResourceCreateHelper::TextureDetails{.ClearValue = {0, 0, 0, 0}});
	auto reflectionResultBuf = graphBuilder.AddGraphResource("ReflectionResultBuffer", reflectionResultBufferInfo);

	auto refractionResultBuf = graphBuilder.AddGraphResource("RefractionResultBuffer", reflectionResultBufferInfo);

	auto outputBufInfo =
		rad::ResourceCreateHelper::Texture2D(sceneData.View.Width,
											 sceneData.View.Height,
											 DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
											 ResourcePresetFlags::RenderTarget,
											 ResourceCreateHelper::TextureDetails{.ClearValue = {0, 0, 0, 1}});

	auto outputBuf = graphBuilder.AddGraphResource("OutputBuffer", outputBufInfo);
	auto lightingResultBuf = graphBuilder.AddGraphResource("LightingResultBuffer", outputBufInfo);

	auto lightBuf = graphBuilder.AddGraphResource(
		"LightBuffer",
		rad::ResourceCreateHelper::Buffer(sizeof(rad::hlsl::LightDataBuffer), ResourcePresetFlags::ConstantBuffer));

	auto viewTransformBuf = graphBuilder.AddGraphResource(
		"ViewTransformBuffer",
		rad::ResourceCreateHelper::Buffer(sizeof(rad::hlsl::ViewTransformBuffer), ResourcePresetFlags::ConstantBuffer));

	auto shadowMap = graphBuilder.AddGraphResource(
		"ShadowMap",
		ResourceCreateHelper::Texture2D(1024,
										1024,
										DXGI_FORMAT_D32_FLOAT,
										ResourcePresetFlags::DepthStencil,
										ResourceCreateHelper::TextureDetails{.ClearValue = {1.f, 1.f, 1.f, 1.f}}));

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

	// TODO RenderGraph: Samplers?
	// D3D12_SAMPLER_DESC shadowSampler{};
	// shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	// shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	// shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	// shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	// shadowSampler.MipLODBias = 0;
	// shadowSampler.MaxAnisotropy = 16;
	// shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	// memset(shadowSampler.BorderColor, std::bit_cast<int>(1.0f), sizeof(shadowSampler.BorderColor));
	//
	// Renderer.GetDevice().CreateSampler(&shadowSampler, ShadowMapSampler.GetCPUHandle());

	{
		hlsl::LightDataBuffer lightData{.DirectionOrPosition = sceneData.LightInfo.View.ViewDirection,
										.Color = sceneData.LightInfo.Color,
										.Intensity = sceneData.LightInfo.Intensity,
										.AmbientColor = sceneData.LightInfo.AmbientColor};
		rghelpers::UploadConstantBufferData(graphBuilder, lightBuf, lightData);

		hlsl::ViewTransformBuffer viewTransform{};
		viewTransform.CamView = sceneData.View.ViewMatrix;
		viewTransform.CamProjection = sceneData.View.ProjectionMatrix;
		viewTransform.CamViewProjection = sceneData.View.ViewProjectionMatrix;
		viewTransform.CamInverseView = glm::inverse(sceneData.View.ViewMatrix);
		viewTransform.CamInverseProjection = glm::inverse(sceneData.View.ProjectionMatrix);
		viewTransform.CamInverseViewProjection = viewTransform.CamInverseView * viewTransform.CamInverseProjection;
		viewTransform.CamNear = sceneData.View.NearPlane;
		viewTransform.CamFar = sceneData.View.FarPlane;
		viewTransform.LightViewProjection = sceneData.LightInfo.View.ViewProjectionMatrix;
		rghelpers::UploadConstantBufferData(graphBuilder, viewTransformBuf, viewTransform);
	}

	{
		PreRenderPassData preRenderPassData{.GraphBuilder = graphBuilder, .Frame = sceneData};
		OnPreRenderPass.Broadcast(preRenderPassData);
	}

	{
		rghelpers::ClearDepthStencilView(graphBuilder, shadowMap, 1.0f, std::nullopt);

		// ShadowMap pass
		ShadowMapPassData shadowMapPassData{
			.GraphBuilder = graphBuilder, .Frame = sceneData, .OutShadowMap = shadowMap};

		OnShadowMapPass.Broadcast(shadowMapPassData);
	}

	{
		rghelpers::ClearDepthStencilView(graphBuilder, depthBuf, 1.0f, std::nullopt);
		rghelpers::ClearRenderTargetView(graphBuilder, albedoBuf, {0, 0, 0, 1});
		rghelpers::ClearRenderTargetView(graphBuilder, normalBuf, {0, 0, 0, 1});
		// Deferred Render Pass
		DeferredPassData deferredPassData{.GraphBuilder = graphBuilder,
										  .Frame = sceneData,
										  .OutAlbedoBuffer = albedoBuf,
										  .OutNormalBuffer = normalBuf,
										  .OutDepthBuffer = depthBuf};
		OnDeferredPass.Broadcast(deferredPassData);
	}
	{
		rghelpers::CopyResource(graphBuilder, depthBuf, ssDepth);
		rghelpers::ClearRenderTargetView(graphBuilder, reflectRefractBuf, {0, 0, 0, 0});
		// Water Render Pass
		WaterPassData waterPassData{.GraphBuilder = graphBuilder,
									.Frame = sceneData,
									.OutReflectionRefraction = reflectRefractBuf,
									.OutDepthBuffer = ssDepth,
									.InViewTransform = viewTransformBuf};
		OnWaterPass.Broadcast(waterPassData);
	}
	{
		// Screen Space Raymarch Pass

		auto& ssRaymarchPass = graphBuilder.AddPass("ScreenSpaceRaymarchPass");
		auto inReflectRefract = ssRaymarchPass.AddInput(
			"ReflectRefractNormal", reflectRefractBuf, RGResourceUsage::ShaderResourceView(reflectRefractBuf));
		auto inSSDepth = ssRaymarchPass.AddInput("SSDepth", ssDepth, RGResourceUsage::ShaderResourceView(ssDepth));
		auto inDepth = ssRaymarchPass.AddInput("Depth", depthBuf, RGResourceUsage::ShaderResourceView(depthBuf));
		auto outReflectionResult = ssRaymarchPass.AddInResourceSetOut(
			"ReflectionResult", reflectionResultBuf, RGResourceUsage::UnorderedAccessView(reflectionResultBuf));
		auto outRefractionResult = ssRaymarchPass.AddInResourceSetOut(
			"RefractionResult", refractionResultBuf, RGResourceUsage::UnorderedAccessView(refractionResultBuf));
		auto inViewTransform = ssRaymarchPass.AddInput(
			"ViewTransformBuffer", viewTransformBuf, RGResourceUsage::ConstantBufferView(viewTransformBuf));

		ssRaymarchPass.Execute =
			[inReflectRefract, inSSDepth, inDepth, outReflectionResult, outRefractionResult, inViewTransform, this](
				CommandContext& cmdContext) {
				hlsl::ScreenSpaceRaymarchResources resources{};

				resources.InReflectRefractNormalTextureIndex =
					inReflectRefract->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
				resources.SSDepthTextureIndex =
					inSSDepth->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
				resources.DepthTextureIndex =
					inDepth->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
				resources.OutReflectResultTextureIndex =
					outReflectionResult->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index;
				resources.OutRefractResultTextureIndex =
					outRefractionResult->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index;
				resources.ViewTransformBufferIndex =
					inViewTransform->GetResourceView().AsGPUDescriptor<ConstantBufferViewDesc>().Index;

				uint32_t width = inReflectRefract->GetResourceView().GetCreateInfo().Desc.Width;
				uint32_t height = inReflectRefract->GetResourceView().GetCreateInfo().Desc.Height;

				ScreenSpaceRaymarchPipelineState.ExecuteCompute(
					cmdContext, resources, (width + 7) / 8, (height + 7) / 8, 1);
			};
	}
	{
		// Lighting Pass
		auto& lightingPass = graphBuilder.AddPass("LightingPass");
		auto outLightingResult = lightingPass.AddInResourceSetOut(
			"LightingResult", lightingResultBuf, RGResourceUsage::RenderTargetView(lightingResultBuf));
		auto inAlbedo = lightingPass.AddInput("Albedo", albedoBuf, RGResourceUsage::ShaderResourceView(albedoBuf));
		auto inNormal = lightingPass.AddInput("Normal", normalBuf, RGResourceUsage::ShaderResourceView(normalBuf));
		auto inDepth = lightingPass.AddInput("Depth", depthBuf, RGResourceUsage::ShaderResourceView(depthBuf));
		auto inShadowMap =
			lightingPass.AddInput("ShadowMap", shadowMap, RGResourceUsage::ShaderResourceView(shadowMap));
		// TODO RenderGraph: Sampler
		auto inLightBuffer =
			lightingPass.AddInput("LightBuffer", lightBuf, RGResourceUsage::ConstantBufferView(lightBuf));
		auto inViewTransform = lightingPass.AddInput(
			"ViewTransformBuffer", viewTransformBuf, RGResourceUsage::ConstantBufferView(viewTransformBuf));
		auto inReflectionResult = lightingPass.AddInput(
			"ReflectionResult", reflectionResultBuf, RGResourceUsage::ShaderResourceView(reflectionResultBuf));
		auto inRefractionResult = lightingPass.AddInput(
			"RefractionResult", refractionResultBuf, RGResourceUsage::ShaderResourceView(refractionResultBuf));
		lightingPass.Execute = [outLightingResult,
								inAlbedo,
								inNormal,
								inDepth,
								inShadowMap,
								inLightBuffer,
								inViewTransform,
								inReflectionResult,
								inRefractionResult,
								this](CommandContext& cmdContext) {
			auto rtv = outLightingResult->GetResourceView().AsCPUDescriptor<RenderTargetViewDesc>().GetCPUHandle();
			cmdContext->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

			rad::hlsl::LightingResources lightingResources{};
			lightingResources.AlbedoTextureIndex =
				inAlbedo->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			lightingResources.NormalTextureIndex =
				inNormal->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			lightingResources.DepthTextureIndex =
				inDepth->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			lightingResources.ShadowMapTextureIndex =
				inShadowMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			lightingResources.ShadowMapSamplerIndex = ShadowMapSampler.Index;
			lightingResources.LightDataBufferIndex =
				inLightBuffer->GetResourceView().AsGPUDescriptor<ConstantBufferViewDesc>().Index;
			lightingResources.ViewTransformBufferIndex =
				inViewTransform->GetResourceView().AsGPUDescriptor<ConstantBufferViewDesc>().Index;
			lightingResources.ReflectionResultIndex =
				inReflectionResult->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			lightingResources.RefractionResultIndex =
				inRefractionResult->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			LightingPipelineState.BindWithResources(cmdContext, lightingResources);
			cmdContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmdContext->DrawInstanced(4, 1, 0, 0);
		};
	}
	{
		// Forward Render Pass
		ForwardPassData forwardPassData{.GraphBuilder = graphBuilder,
										.Frame = sceneData,
										.InOutColor = outputBuf,
										.InOutSSDepth = ssDepth,
										.InViewTransform = viewTransformBuf,
										.InOpaquaDepth = depthBuf,
										.InReflectionResult = reflectionResultBuf,
										.InRefractionResult = refractionResultBuf};
		OnForwardRenderPass.Broadcast(forwardPassData);
	}
}
} // namespace rad