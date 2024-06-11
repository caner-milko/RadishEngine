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

struct StaticMeshPipeline
{
	bool Setup(ID3D12Device2* dev);
	
	bool Run(ID3D12GraphicsCommandList2* cmd, ViewData const& viewData, SceneDataView const& scene, FrameContext& frameCtx);

	std::unique_ptr<DXBuffer> LightBuffer;
	void* LightBufferPtr;

	RootSignature RootSignature;
	PipelineState PipelineState;
};

}