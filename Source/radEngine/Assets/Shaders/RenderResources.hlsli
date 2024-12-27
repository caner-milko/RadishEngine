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

    uint ReflectionRefractionResultIndex;
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
    uint ViewTransformBufferIndex;
    
    float MaxDistance DEFAULT_VALUE(1.5f);
    float Resolution DEFAULT_VALUE(0.3f);
    float Thickness DEFAULT_VALUE(0.5f);
    int MaxSteps DEFAULT_VALUE(16);
    
};
#ifdef __cplusplus
};
};
#endif