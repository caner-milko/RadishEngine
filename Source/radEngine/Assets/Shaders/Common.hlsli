#pragma once

#ifdef __cplusplus

#define float4 DirectX::XMFLOAT4
#define float3 DirectX::XMFLOAT3
#define float2 DirectX::XMFLOAT2

#define uint uint32_t

// Note : using the typedef of matrix (float4x4) and struct (ConstantBufferStruct) to prevent name collision on the cpp
// code side.
#define float4x4 DirectX::XMMATRIX


#define DEFAULT_VALUE(x) = x
#else
// if HLSL
#define PI 3.14159265359

#define DEFAULT_VALUE(x) 
#endif

#if RAD_BINDLESS
#define GetBindlessResource(index) ResourceDescriptorHeap[index]
#define GetBindlessSampler(index) SamplerDescriptorHeap[index]
#endif
