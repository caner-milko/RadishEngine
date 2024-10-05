#pragma once

#include "Common.hlsli"

#ifdef __cplusplus
namespace rad
{
namespace hlsl
{
#endif
    
struct StaticMeshResources
{
    float4x4 MVP;
    float4x4 Normal;
    uint MaterialBufferIndex;
};

struct ShadowMapResources
{
    float4x4 MVP;
};
    
struct LightingResources
{
    uint AlbedoTextureIndex;
    uint NormalTextureIndex;
    uint DepthTextureIndex;
        
    uint ShadowMapTextureIndex;
    uint ShadowMapSamplerIndex;
    uint LightDataBufferIndex;
    uint LightTransformBufferIndex;
};
    
struct BlitResources
{
    uint SourceTextureIndex;
};
#ifdef __cplusplus
};
};
#endif