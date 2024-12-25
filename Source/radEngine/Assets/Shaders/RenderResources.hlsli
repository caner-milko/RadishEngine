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

struct ScreenSpaceRaymarchResources
{
    // RG - Reflection Normal, BA - Refraction Normal
    uint InReflectRefractNormalTextureIndex;
    uint SSDepthTextureIndex;
    uint DepthTextureIndex;
    
    // RG - Reflection UV, BA - Refraction UV
    uint OutReflectRefractResultTextureIndex;
    // Contains camera information
    uint LightTransformBufferIndex;
    
    float MaxDistance = 15;
    float Resolution = 0.3;
    float Thickness = 0.5;
    int MaxSteps = 16;
    
};
#ifdef __cplusplus
};
};
#endif