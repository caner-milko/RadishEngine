#pragma once

#include "RadishCommon.h"
#include "DXResource.h"
#include "RendererCommon.h"
#include "Model.h"
#include "PipelineState.h"

namespace rad::proc
{

struct TerrainData
{
	uint32_t MeshResX = 256, MeshResY = 256;
	std::optional<StandaloneModel> Model = std::nullopt;
	std::optional<Material> Material{};

	DXTexture HeightMap{};
	DXTexture AlbedoTex{};
	DXTexture NormalMap{};
	DescriptorAllocation HeightMapSRV{};
	DescriptorAllocation AlbedoTexSRV{};
	DescriptorAllocation NormalMapSRV{};
	DescriptorAllocation HeightMapUAV{};
	DescriptorAllocation AlbedoTexUAV{};
	DescriptorAllocation NormalMapUAV{};
	DescriptorAllocation VerticesUAV{};
};

struct TerrainGenerator
{
	bool Setup(ID3D12Device2* dev);

	std::vector<float> CreateDiamondSquareHeightMap(uint32_t toPowerOfTwo, uint32_t& width);
	TerrainData InitializeTerrain(ID3D12Device* device, uint32_t resX, uint32_t resY);
	void GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, uint32_t toPowerOfTwo);
	void ErodeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void InitializeMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void InitializeMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void GenerateMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void GenerateMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	
	ID3D12Device2* Device = nullptr;
	PipelineState HeightMapToMeshPSO;
	PipelineState HeightMapToMaterialPSO;
	PipelineState ThermalOutfluxPSO;
	PipelineState ThermalDepositPSO;
};

}