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
};

struct ThermalDepositResources
{
    uint InFluxTextureIndex1;
    uint InFluxTextureIndex2;
    uint OutHeightMapIndex;
};
 
#ifdef __cplusplus
};
};
#endif