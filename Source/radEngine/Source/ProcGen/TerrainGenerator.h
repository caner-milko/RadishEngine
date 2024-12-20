#pragma once

#include "RadishCommon.h"
#include "Graphics/DXResource.h"
#include "Graphics/RendererCommon.h"
#include "Graphics/Model.h"
#include "Graphics/PipelineState.h"
#include "Compute/Terrain/TerrainResources.hlsli"
#include "InputManager.h"
#include "Graphics/Renderer.h"
#include "entt/entt.hpp"

namespace rad::proc
{

struct RWTexture : public DXTexture
{
	RWTexture() = default;
	RWTexture(DXTexture texture, int srvMipLevels = 1);
	DescriptorAllocation UAV{};
	DescriptorAllocation SRV{};
};

struct CTerrain
{
	std::shared_ptr<RWTexture> HeightMap{};

	std::shared_ptr<RWTexture> WaterHeightMap{};

	std::shared_ptr<RWTexture> TempHeightMap{};
	std::shared_ptr<RWTexture> SedimentMap{};
	std::shared_ptr<RWTexture> TempSedimentMap{};
	std::shared_ptr<RWTexture> WaterOutflux{};
	std::shared_ptr<RWTexture> VelocityMap{};
	std::shared_ptr<RWTexture> ThermalPipe1{};
	std::shared_ptr<RWTexture> ThermalPipe2{};
	std::shared_ptr<RWTexture> SoftnessMap{};
	uint32_t IterationCount = 0;
};

struct CIndexedPlane
{
	uint32_t ResX = 256, ResY = 256;
	std::shared_ptr<DXTypedBuffer<uint32_t>> Indices;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView{};
};

struct CTerrainRenderable
{
	std::shared_ptr<RWTexture> HeightMap{};
	std::shared_ptr<RWTexture> TerrainAlbedoTex{};
	std::shared_ptr<RWTexture> TerrainNormalMap{};
	float TotalLength = 1024.0f;
};

struct CWaterRenderable
{
	std::shared_ptr<RWTexture> HeightMap{};
	std::shared_ptr<RWTexture> WaterHeightMap{};
	std::shared_ptr<RWTexture> WaterAlbedoMap{};
	std::shared_ptr<RWTexture> WaterNormalMap{};
	float TotalLength = 1024.0f;
};

struct CErosionParameters
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

struct TerrainErosionSystem
{
	TerrainErosionSystem(Renderer& renderer) : Renderer(renderer) {}
	bool Setup();

	std::vector<float> CreateDiamondSquareHeightMap(uint32_t width, float roughness);
	CTerrain CreateTerrain(uint32_t heightMapWidth);
	CIndexedPlane CreatePlane(CommandRecord& cmdRecord, uint32_t resX, uint32_t resY);
	CTerrainRenderable CreateTerrainRenderable(CTerrain& terrain);
	CWaterRenderable CreateWaterRenderable(CTerrain& terrain);
	void GenerateBaseHeightMap(CommandRecord& cmdRecord, CTerrain& terrain, CErosionParameters const& parameters,
							   OptionalRef<CTerrainRenderable> terrainRenderable,
							   OptionalRef<CWaterRenderable> waterRenderable);
	void ErodeTerrain(CommandRecord& cmdRecord, CTerrain& terrain, CErosionParameters const& parameters,
					  OptionalRef<CTerrainRenderable> terrainRenderable, OptionalRef<CWaterRenderable> waterRenderable);
	void GenerateTerrainMaterial(CommandRecord& cmdRecord, CTerrain& terrain, CErosionParameters const& parameters,
								 CTerrainRenderable& terrainRenderable);
	void GenerateWaterMaterial(CommandRecord& cmdRecord, CTerrain& terrain, CErosionParameters const& parameters,
							   CWaterRenderable& waterRenderable);

	void Update(entt::registry& registry, InputManager& inputMan, RenderFrameRecord& frameRecord);

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
	GraphicsPipelineState<hlsl::WaterRenderResources> WaterForwardPSO;

	struct TerrainRenderData
	{
		glm::mat4 WorldMatrix;
		uint32_t IndexCount;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
		hlsl::TerrainRenderResources Resources;
	};

	void TerrainDepthOnlyPass(std::span<TerrainRenderData> renderObjects, const RenderView& view,
							  DepthOnlyPassData& passData);
	void TerrainDeferredPass(std::span<TerrainRenderData> renderObjects, const RenderView& view,
							 DeferredPassData& passData);

	struct WaterRenderData
	{
		glm::mat4 WorldMatrix;
		uint32_t IndexCount;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
		hlsl::WaterRenderResources Resources;
	};

	void WaterForwardPass(std::span<WaterRenderData> renderObjects, const RenderView& view, ForwardPassData& passData);
};

} // namespace rad::proc
