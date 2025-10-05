#pragma once

#include "EngineCommon.h"
#include "Graphics/DXResource.h"
#include "Graphics/RendererCommon.h"
#include "Graphics/Model.h"
#include "Graphics/PipelineState.h"
#include "Compute/Terrain/TerrainResources.hlsli"
#include "InputManager.h"
#include "entt/entt.hpp"
#include "Graphics/RenderGraph.h"
#include "Graphics/ResourcePool.h"

namespace rad::proc
{

struct CTerrain
{
	Ref<ResourcePool::OwnedResource> HeightMap;
	Ref<ResourcePool::OwnedResource> WaterHeightMap;

	Ref<ResourcePool::OwnedResource> SedimentMap;
	Ref<ResourcePool::OwnedResource> WaterOutflux;
	Ref<ResourcePool::OwnedResource> VelocityMap;
	Ref<ResourcePool::OwnedResource> ThermalPipe1;
	Ref<ResourcePool::OwnedResource> ThermalPipe2;
	Ref<ResourcePool::OwnedResource> HardnessMap;
	uint32_t IterationCount = 0;
};

struct CIndexedPlane
{
	uint32_t ResX = 256, ResY = 256;
	Ref<ResourcePool::OwnedResource> Indices;
};

struct CTerrainRenderable
{
	PoolResourceView HeightMap;
	Ref<ResourcePool::OwnedResource> TerrainAlbedoTex;
	Ref<ResourcePool::OwnedResource> TerrainNormalMap;
	float TotalLength = 1024.0f;
};

struct CWaterRenderable
{
	PoolResourceView HeightMap;
	PoolResourceView WaterHeightMap;
	Ref<ResourcePool::OwnedResource> WaterAlbedoMap;
	Ref<ResourcePool::OwnedResource> WaterNormalMap;
	float TotalLength = 1024.0f;
};

struct CErosionParameters
{
	bool ErodeEachFrame = true;
	bool Random = false;
	int Seed = 0;
	bool BaseFromFile = false;
	float InitialRoughness = 2.0f;
	float MinHeight = 0.0f;
	float MaxHeight = 200.0f;
	float DeltaTime = 0.002f;
	int Iterations = 1;
	float RainRate = 0.04f;
	float EvaporationRate = 0.02f;
	float TotalLength = 1024.0;
	float PipeCrossSection = 4.0f;
	float SedimentCapacity = 1.0f;
	float SoilSuspensionRate = 0.3f;
	float SedimentDepositionRate = 0.8f;
	float SoilHardeningRate = 0.5f;
	float MaximumSoilHardness = 1.0f;
	float MaximalErosionDepth = 10.0f;

	float TalusAngleTangentCoeff = 0.8f;
	float TalusAngleTangentBias = 0.3f;
	float ThermalErosionRate = 0.1f;
	bool MeshWithWater = false;
};

struct TerrainErosionSystem
{
	TerrainErosionSystem(Renderer& renderer) : Renderer(renderer) {}
	bool Setup();

	std::vector<float> CreateDiamondSquareHeightMap(uint32_t width, float roughness);
	CTerrain CreateTerrain(uint32_t heightMapWidth);
	CIndexedPlane CreatePlane(RenderGraphBuilder& rgBuilder, uint32_t resX, uint32_t resY);
	CTerrainRenderable CreateTerrainRenderable(CTerrain& terrain);
	CWaterRenderable CreateWaterRenderable(CTerrain& terrain);
	void GenerateBaseHeightMap(RenderGraphBuilder& rgBuilder,
							   CTerrain& terrain,
							   CErosionParameters const& parameters,
							   OptionalRef<CTerrainRenderable> terrainRenderable,
							   OptionalRef<CWaterRenderable> waterRenderable);
	void ErodeTerrain(RenderGraphBuilder& rgBuilder,
					  CTerrain& terrain,
					  CErosionParameters const& parameters,
					  OptionalRef<CTerrainRenderable> terrainRenderable,
					  OptionalRef<CWaterRenderable> waterRenderable);
	void GenerateTerrainMaterial(RenderGraphBuilder& rgBuilder,
								 CTerrain& terrain,
								 CErosionParameters const& parameters,
								 CTerrainRenderable& terrainRenderable);
	void GenerateWaterMaterial(RenderGraphBuilder& rgBuilder,
							   CTerrain& terrain,
							   CErosionParameters const& parameters,
							   CWaterRenderable& waterRenderable);

	void Update(entt::registry& registry, InputManager& inputMan);

private:
	Renderer& Renderer;
	ComputePipelineState<hlsl::HeightToTerrainMaterialResources> HeightMapToTerrainMaterialPSO;
	ComputePipelineState<hlsl::HeightToWaterMaterialResources> HeightMapToWaterMaterialPSO;
	ComputePipelineState<hlsl::ThermalOutfluxResources> ThermalOutfluxPSO;
	ComputePipelineState<hlsl::ThermalDepositResources> ThermalDepositPSO;

	ComputePipelineState<hlsl::HydrolicAddWaterResources> HydrolicAddWaterPSO;
	ComputePipelineState<hlsl::HydrolicCalculateOutfluxResources> HydrolicCalculateOutfluxPSO;
	ComputePipelineState<hlsl::HydrolicUpdateWaterVelocityResources> HydrolicUpdateWaterVelocityPSO;
	ComputePipelineState<hlsl::HydrolicErosionAndDepositionResources> HydrolicErosionAndDepositionPSO;
	ComputePipelineState<hlsl::HydrolicSedimentTransportationAndEvaporationResources>
		HydrolicSedimentTransportationAndEvaporationPSO;

	GraphicsPipelineState<hlsl::TerrainRenderResources> TerrainDeferredPSO;
	GraphicsPipelineState<hlsl::TerrainRenderResources> TerrainDepthOnlyPSO;
	GraphicsPipelineState<hlsl::WaterRenderResources> WaterPrePassPSO;
	GraphicsPipelineState<hlsl::WaterRenderResources> WaterForwardPSO;

	struct TerrainRenderData
	{
		glm::mat4 WorldMatrix;
		uint32_t IndexCount;
		PoolResourceView Indices;
		PoolResourceView HeightMap;
		PoolResourceView TerrainAlbedoTex;
		PoolResourceView TerrainNormalMap;
		hlsl::TerrainRenderResources Resources;
	};

	std::vector<TerrainRenderData> FrameTerrainRenderData;

	void TerrainShadowMapPass(ShadowMapPassData& passData);
	void TerrainDeferredPass(DeferredPassData& passData);

	struct WaterRenderData
	{
		glm::mat4 WorldMatrix;
		uint32_t IndexCount;
		PoolResourceView Indices;
		PoolResourceView HeightMap;
		PoolResourceView WaterHeightMap;
		PoolResourceView WaterAlbedoMap;
		PoolResourceView WaterNormalMap;
		hlsl::WaterRenderResources Resources;
	};

	std::vector<WaterRenderData> FrameWaterRenderData;

	void WaterPass(WaterPassData& passData);
	void WaterForwardPass(ForwardPassData& passData);
};

} // namespace rad::proc
