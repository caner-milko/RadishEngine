#pragma once

#include "RenderResources.hlsli"

#ifdef __cplusplus
namespace rad
{
namespace hlsl
{
#endif

struct HeightToMaterialResources
{
    uint HeightMapTextureIndex;
    uint AlbedoTextureIndex;
    uint NormalMapTextureIndex;
};
    
struct HeightToMeshResources
{
    uint HeightMapTextureIndex;
    uint WaterHeightMapTextureIndex;
    uint VertexBufferIndex;
    uint MeshResX, MeshResY;
    uint WithWater;
};

#define EROSION_DELTA_TIME 0.02f

struct ThermalOutfluxResources
{
    uint InHeightMapIndex;
    uint OutFluxTextureIndex1;
    uint OutFluxTextureIndex2;
    float ThermalErosionRate DEFAULT_VALUE(0.15);
    float TalusAnglePrecomputed DEFAULT_VALUE(0.577);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
    float PipeLength DEFAULT_VALUE(0.8f);
};

struct ThermalDepositResources
{
    uint InFluxTextureIndex1;
    uint InFluxTextureIndex2;
    uint OutHeightMapIndex;
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};
 
struct HydrolicAddWaterResources
{
    uint WaterMapIndex;
    float RainRate DEFAULT_VALUE(0.001f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
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
    uint OutHeightMapIndex;
    uint OutWaterMapIndex;
    uint OutSedimentMapIndex;
    float SedimentCapacity DEFAULT_VALUE(.06f);
    float SoilSuspensionRate DEFAULT_VALUE(0.036f);
    float SedimentDepositionRate DEFAULT_VALUE(0.006f);
    float SedimentSofteningRate DEFAULT_VALUE(5.0f);
    float MaximalErosionDepth DEFAULT_VALUE(1.0f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

struct HydrolicSedimentTransportationAndEvaporationResources
{
    // For sediment transportation
    uint InVelocityMapIndex;
    uint InOldSedimentMapIndex;
    uint OutSedimentMapIndex;
    // For evaporation
    uint InOutWaterMapIndex;
    float EvaporationRate DEFAULT_VALUE(0.015f);
    float DeltaTime DEFAULT_VALUE(EROSION_DELTA_TIME);
};

#ifdef __cplusplus
};
};
#endif