// clang-format off
#pragma once
#include "Common.hlsli"

// To be able to share Constant Buffer Types (Structs) between HLSL and C++, a few defines are set here.
// Define the Structs in HLSLS syntax, and with this defines the C++ application can also use the structs.
#ifdef __cplusplus

#define ConstantBufferStruct struct alignas(256)

#else
// if HLSL

#define ConstantBufferStruct struct

#endif

#ifdef __cplusplus
namespace rad
{
namespace hlsl
{
#endif

ConstantBufferStruct TransformBuffer
{
	float4x4 MVP;
	float4x4 Normal;
};

ConstantBufferStruct MaterialBuffer
{
    float4 Diffuse;
	// Do not sample if 0
    uint DiffuseTextureIndex;
    uint NormalMapTextureIndex;
};
    
ConstantBufferStruct LightDataBuffer
{
    float3 DirectionOrPosition;
    float Padding;
    float3 Color;
    float Intensity;
    float3 AmbientColor;
    float Padding2;
};

ConstantBufferStruct ViewTransformBuffer
{
    float4x4 CamView;
    float4x4 CamProjection;
    float4x4 CamViewProjection;
    float4x4 CamInverseView;
    float4x4 CamInverseProjection;
    float4x4 CamInverseViewProjection;
    float CamNear;
    float CamFar;
    float2 _padding;
    
    float4x4 LightViewProjection;
};

ConstantBufferStruct Vertex
{
    float3 Position;
    float3 Normal;
    float2 TexCoord;
    float3 Tangent;
};
    
#ifdef __cplusplus
};
};
#endif