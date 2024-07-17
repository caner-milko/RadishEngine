#pragma once

#include "RootSignature.h"
#include "PipelineState.h"

#include "RendererCommon.h"
#include "DXResource.h"

namespace dxpg
{
struct DeferredRenderingPipeline
{
	bool Setup(ID3D12Device2* dev);
	void OnResize(uint32_t width, uint32_t height);

	void Run(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx);

	DXTexture& GetOutputBuffer() { return OutputBuffer; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetOutputBufferSRV() { return OutputBufferSRV.GetGPUHandle(); }
	DXTexture& GetShadowMap() { return ShadowMap; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSRV() { return ShadowMapSRV.GetGPUHandle(); }
	DXTexture& GetAlbedoBuffer() { return AlbedoBuffer; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetAlbedoBufferSRV() { return GBuffersSRV.GetGPUHandle(); }
	DXTexture& GetNormalBuffer() { return NormalBuffer; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetNormalBufferSRV() { return GBuffersSRV.GetGPUHandle(1); }
private:
	bool SetupStaticMeshPipeline();
	bool SetupLightingPipeline();
	bool SetupShadowMapPipeline();

	void RunStaticMeshPipeline(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene);
	void RunShadowMapPipeline(ID3D12GraphicsCommandList2* cmd, SceneDataView const& scene);
	void RunLightingPipeline(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx);



	ID3D12Device2* Device;
	RootSignature StaticMeshRootSignature;
	PipelineState StaticMeshPipelineState;

	DXTexture DepthBuffer;
	DXTexture AlbedoBuffer;
	DXTexture NormalBuffer;

	DescriptorAllocation DepthBufferDSV;
	DescriptorAllocation AlbedoBufferRTV;
	DescriptorAllocation NormalBufferRTV;
	DescriptorAllocation GBuffersSRV;

	RootSignature ShadowMapRootSignature;
	PipelineState ShadowMapPipelineState;
	DXTexture ShadowMap;
	DescriptorAllocation ShadowMapDSV;
	DescriptorAllocation ShadowMapSRV;

	DXBuffer LightBuffer;
	DXBuffer LightTransformationMatricesBuffer;
	RootSignature LightingRootSignature;
	PipelineState LightingPipelineState;
	DXTexture OutputBuffer;
	DescriptorAllocation OutputBufferRTV;
	DescriptorAllocation OutputBufferSRV;

	D3D12_VIEWPORT ShadowMapViewport;
	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;
};

}