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
	std::optional<StandaloneModel> TerrainModel = std::nullopt;
	std::optional<Material> TerrainMaterial{};
	std::optional<StandaloneModel> WaterModel = std::nullopt;
	std::optional<Material> WaterMaterial{};

	RWTexture HeightMap{};
	RWTexture TerrainAlbedoTex{};
	RWTexture TerrainNormalMap{};
	DescriptorAllocation TerrainVerticesUAV{};

	RWTexture WaterHeightMap{};
	RWTexture WaterAlbedoTex{};
	RWTexture WaterNormalMap{};
	DescriptorAllocation WaterVerticesUAV{};

	RWTexture TempHeightMap{};
	RWTexture SedimentMap{};
	RWTexture TempSedimentMap{};
	RWTexture WaterOutflux{};
	RWTexture VelocityMap{};
	RWTexture ThermalPipe1{};
	RWTexture ThermalPipe2{};
	RWTexture SoftnessMap{};
	uint32_t IterationCount = 0;
};

struct ErosionParameters
{
	bool ErodeEachFrame = true;
	bool Random = false;
	int Seed = 0;
	bool BaseFromFile = false;
	float InitialRoughness = 4.0f;
	float MinHeight = 0.0f;
	float MaxHeight = 120.0f;
	int Iterations = 1;
	float RainRate = 0.015f;
	float EvaporationRate = 0.006f;
	float TotalLength = 1024.0;
	float PipeCrossSection = 20.0f;
	float SedimentCapacity = 1.0f;
	float SoilSuspensionRate = 0.6f;
	float SedimentDepositionRate = 0.8f;
	float SoilHardeningRate = 0.2f;
	float SoilSofteningRate = 0.2f;
	float MinimumSoilSoftness = 0.0f;
	float MaximalErosionDepth = 10.0f;

	float SoftnessTalusCoefficient = 0.6f;
	float MinTalusCoefficient = 0.3f;
	float ThermalErosionRate = 0.1f;
	bool MeshWithWater = false;
};

struct TerrainGenerator
{
	bool Setup(ID3D12Device2* dev);

	std::vector<float> CreateDiamondSquareHeightMap(uint32_t width, float roughness);
	TerrainData InitializeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, uint32_t resX, uint32_t resY, uint32_t heightMapWidth);
	void GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters, bool generateMeshAndMaterial = true);
	void ErodeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters, bool generateMeshAndMaterial = true);
	void GenerateMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters);
	void GenerateMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters);
	
	ID3D12Device2* Device = nullptr;
	PipelineState HeightMapToMeshPSO;
	PipelineState HeightMapToMaterialPSO;
	PipelineState ThermalOutfluxPSO;
	PipelineState ThermalDepositPSO;

	PipelineState HydrolicAddWaterPSO;
	PipelineState HydrolicCalculateOutfluxPSO;
	PipelineState HydrolicUpdateWaterVelocityPSO;
	PipelineState HydrolicErosionAndDepositionPSO;
	PipelineState HydrolicSedimentTransportationAndEvaporationPSO;

private:
	void InitializeMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
	void InitializeMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain);
};

}