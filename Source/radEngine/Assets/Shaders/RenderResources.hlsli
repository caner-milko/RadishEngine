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
    uint ViewTransformBufferIndex;

    uint ReflectionResultIndex;
    uint RefractionResultIndex;
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
    
    // RG - Reflection UV, A - Visibility
    uint OutReflectResultTextureIndex;
    // RG - Refraction UV, A - Visibility
    uint OutRefractResultTextureIndex;
    // Contains camera information
    uint ViewTransformBufferIndex;
    
    float MaxDistance DEFAULT_VALUE(15.0f);
    float Resolution DEFAULT_VALUE(0.4f);
    float ThicknessMultiplier DEFAULT_VALUE(0.2f);
    int MaxSteps DEFAULT_VALUE(16);
};
#ifdef __cplusplus
};
};
#endif