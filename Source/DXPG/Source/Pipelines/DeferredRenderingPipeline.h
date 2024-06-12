#pragma once

#include "RootSignature.h"
#include "PipelineState.h"

#include "RendererCommon.h"
#include "DXResource.h"

namespace dxpg
{
struct LightData
{
    union
    {
        struct
		{
	        Vector3 Direction;
	        float Padding;
		} Directional;
        struct
		{
			Vector3 Position;
			float Padding;
		} Point;
    };
	Vector3 Color;
	float Intensity;
	Vector3 AmbientColor;
	float Padding2;
};

struct MeshGroupView
{
    struct MeshView
    {
        D3D12_VERTEX_BUFFER_VIEW IndexBufferView;
        size_t IndexCount;
        Matrix4x4 ModelMatrix;

        bool UseDiffuseTexture = false;
        Vector3 DiffuseColor{};
        D3D12_GPU_DESCRIPTOR_HANDLE DiffuseSRV{};

        bool UseAlphaTexture = false;
        D3D12_GPU_DESCRIPTOR_HANDLE AlphaSRV{};
    };
    D3D12_GPU_DESCRIPTOR_HANDLE VertexSRV;
    std::vector<MeshView> Meshes;
};

struct SceneDataView
{
    std::vector<MeshGroupView> MeshGroups;
	LightData Light;
};

struct DeferredRenderingPipeline
{
	bool Setup(ID3D12Device2* dev, uint32_t width, uint32_t height);
	void OnResize(uint32_t width, uint32_t height);

	DXTexture& Run(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx);


private:

	void RunStaticMeshPipeline(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene);
	void RunLightingPipeline(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx);

	std::unique_ptr<DXBuffer> LightBuffer;
	void* LightBufferPtr;

	ID3D12Device2* Device;
	RootSignature StaticMeshRootSignature;
	PipelineState StaticMeshPipelineState;
	RootSignature LightingRootSignature;
	PipelineState LightingPipelineState;

	std::unique_ptr<DXTexture> DepthBuffer;
	std::unique_ptr<DXTexture> AlbedoBuffer;
	std::unique_ptr<DXTexture> NormalBuffer;
	std::unique_ptr<DXTexture> OutputBuffer;

	std::unique_ptr<DescriptorAllocation> DepthBufferDSV;
	std::unique_ptr<DescriptorAllocation> AlbedoBufferRTV;
	std::unique_ptr<DescriptorAllocation> NormalBufferRTV;
	std::unique_ptr<DescriptorAllocation> OutputBufferRTV;
	std::unique_ptr<DescriptorAllocation> GBuffersSRV;
};

}