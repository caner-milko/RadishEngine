#pragma once

#include "RadishCommon.h"
#include "DXResource.h"
#include "RendererCommon.h"
#include "Model.h"
#include "PipelineState.h"

namespace rad::proc
{

struct RWTexture : public DXTexture
{
	RWTexture() = default;
	RWTexture(DXTexture texture, int srvMipLevels = 1);
	DescriptorAllocation UAV{};
	DescriptorAllocation SRV{};
};

struct TerrainData
{
	uint32_t MeshResX = 256, MeshResY = 256;
	std::optional<StandaloneModel> Model = std::nullopt;
	std::optional<Material> Material{};

	RWTexture HeightMap{};
	RWTexture AlbedoTex{};
	RWTexture NormalMap{};
	DescriptorAllocation VerticesUAV{};
	RWTexture TempHeightMap{};
	RWTexture WaterHeightMap{};
	RWTexture SedimentMap{};
	RWTexture TempSedimentMap{};
	RWTexture WaterOutflux{};
	RWTexture VelocityMap{};
};

struct ErosionParameters
{
	float InitialRoughness = 0.5f;
	float CellSize = 256.0f;
	float HeightToWidthRatio = 1.0f;
	float TalusAngleDegrees = 47.5;
	int Iterations = 256;
};

struct TerrainGenerator
{
	bool Setup(ID3D12Device2* dev);

	std::vector<float> CreateDiamondSquareHeightMap(uint32_t width, float roughness);
	TerrainData InitializeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, uint32_t resX, uint32_t resY, uint32_t heightMapWidth);
	void GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, float roughness, bool generateMeshAndMaterial = true);
	void ErodeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters parameters, bool generateMeshAndMaterial = true);
	void GenerateMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void GenerateMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	
	ID3D12Device2* Device = nullptr;
	PipelineState HeightMapToMeshPSO;
	PipelineState HeightMapToMaterialPSO;
	PipelineState ThermalOutfluxPSO;
	PipelineState ThermalDepositPSO;

private:
	void InitializeMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void InitializeMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
};

}