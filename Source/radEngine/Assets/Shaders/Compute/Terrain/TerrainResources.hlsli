#pragma once

#include "RenderResources.hlsli"

#ifdef __cplusplus
namespace rad
{
namespace hlsl
{
#endif

struct HeightToTerrainMaterialResources
{
    uint HeightMapTextureIndex;
    uint TerrainAlbedoTextureIndex;
    uint TerrainNormalMapTextureIndex;
    float TotalLength DEFAULT_VALUE(1024.0f);
};

struct HeightToWaterMaterialResources
{
    uint HeightMapTextureIndex;
    uint WaterHeightMapTextureIndex;
    uint SedimentMapTextureIndex;
    uint WaterAlbedoTextureIndex;
    uint WaterNormalMapTextureIndex;
    float TotalLength DEFAULT_VALUE(1024.0f);
};
    
struct TerrainRenderResources
{
    float4x4 MVP;
    float4x4 Normal;
    uint MeshResX, MeshResY;
    uint HeightMapTextureIndex;
    uint TerrainAlbedoTextureIndex;
    uint TerrainNormalMapTextureIndex;
    float TotalLength DEFAULT_VALUE(1024.0f);
};

struct WaterRenderResources
{
    float4x4 MVP;
    float4x4 Normal;
    float4 ViewDir;
    uint MeshResX, MeshResY;
    uint HeightMapTextureIndex;
    uint WaterHeightMapTextureIndex;
    uint WaterAlbedoTextureIndex;
    uint WaterNormalMapTextureIndex;
    float TotalLength DEFAULT_VALUE(1024.0f);
};

#define EROSION_DELTA_TIME 0.02f

struct ThermalOutfluxResources
{
    uint InHeightMapIndex;
    uint InHardnessMapIndex;
    uint OutFluxTextureIndex1;
    uint OutFluxTextureIndex2;
    float ThermalErosionRate DEFAULT_VALUE(0.15);
    float PipeLength DEFAULT_VALUE(1.0f);
    float SoftnessTalusCoefficient DEFAULT_VALUE(0.8f);
    float MinTalusCoefficient DEFAULT_VALUE(0.1f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

struct ThermalDepositResources
{
    uint InFluxTextureIndex1;
    uint InFluxTextureIndex2;
    uint OutHeightMapIndex;
};
 
struct HydrolicAddWaterResources
{
    uint WaterMapIndex;
    float RainRate DEFAULT_VALUE(0.001f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
    uint Iteration;
};

struct HydrolicCalculateOutfluxResources
{
    uint InHeightMapIndex;
    uint InWaterMapIndex;
    uint OutFluxTextureIndex;
    float Gravity DEFAULT_VALUE(9.81f);
    float PipeCrossSection DEFAULT_VALUE(20.f);
    float PipeLength DEFAULT_VALUE(0.8f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

struct HydrolicUpdateWaterVelocityResources
{
    uint InFluxTextureIndex;
    uint OutWaterMapIndex;
    uint OutVelocityMapIndex;
    float PipeLength DEFAULT_VALUE(0.8f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

struct HydrolicErosionAndDepositionResources
{
    uint InVelocityMapIndex;
    uint InOldHeightMapIndex;
    uint InOutSoftnessMapIndex;
    uint OutHeightMapIndex;
    uint OutWaterMapIndex;
    uint OutSedimentMapIndex;
    float PipeLength DEFAULT_VALUE(0.8f);
    float SedimentCapacity DEFAULT_VALUE(1.0f);
    float SoilSuspensionRate DEFAULT_VALUE(0.5f);
    float SedimentDepositionRate DEFAULT_VALUE(1.0f);
    float SoilHardeningRate DEFAULT_VALUE(0.1f);
    float SoilSofteningRate DEFAULT_VALUE(0.3f);
    float MinimumSoftness DEFAULT_VALUE(0.01f);
    float MaximalErosionDepth DEFAULT_VALUE(1.0f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

struct HydrolicSedimentTransportationAndEvaporationResources
{
    // For sediment transportation
    uint InVelocityMapIndex;
    uint InOldSedimentMapIndex;
    uint OutSedimentMapIndex;
    float PipeLength DEFAULT_VALUE(0.8f);
    // For evaporation
    uint InOutWaterMapIndex;
    float EvaporationRate DEFAULT_VALUE(0.015f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

#ifdef __cplusplus
};
};
#endif