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
    uint VertexBufferIndex;
    uint MeshResX, MeshResY;
};
        
struct ThermalOutfluxResources
{
    uint InHeightMapIndex;
    uint OutFluxTextureIndex1;
    uint OutFluxTextureIndex2;
    float CellSize DEFAULT_VALUE(256.0f);
    float HeightToWidthRatio DEFAULT_VALUE(2.0f);
    float TalusAnglePrecomputed DEFAULT_VALUE(0.577);
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
    float RainRate DEFAULT_VALUE(0.05f);
};

struct HydrolicCalculateOutfluxResources
{
    uint InHeightMapIndex;
    uint InWaterMapIndex;
    uint OutFluxTextureIndex;
    float Gravity DEFAULT_VALUE(9.81f);
    float PipeCrossSection DEFAULT_VALUE(20.0f);
    float PipeLength DEFAULT_VALUE(1.0f);
};

struct HydrolicUpdateWaterHeightResources
{
    uint InFluxTextureIndex1;
    uint InFluxTextureIndex2;
    uint OutWaterMapIndex;
    float CellSize DEFAULT_VALUE(256.0f);
    float HeightToWidthRatio DEFAULT_VALUE(2.0f);
};

struct HydrolicUpdateWaterVelocityResources
{
    uint InFluxTextureIndex1;
    uint InFluxTextureIndex2;
    uint OutWaterVelocityMapIndex;
};

struct HydrolicSedimentDepositionResources
{
    uint InWaterVelocityMapIndex;
    uint InHeightMapIndex;
    uint OutHeightMapIndex;
    uint OutSedimentMapIndex;
    uint OutWaterMapIndex;
    float SedimentCapacity DEFAULT_VALUE(1.0f);
    float MaximalErosionDepth DEFAULT_VALUE(10.0f);
    float SoilSuspensionRate DEFAULT_VALUE(0.5f);
    float SedimentDepositionRate DEFAULT_VALUE(1.0f);
    float SedimentSofteningRate DEFAULT_VALUE(5.0f);
    float MinSedimentHardness DEFAULT_VALUE(0.1f);
};



#ifdef __cplusplus
};
};
#endif