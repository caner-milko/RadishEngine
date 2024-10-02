#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"

ConstantBuffer<rad:: HeightToAlbedoResources> Resources : register(b0);

uint2 ClampTexCoord(uint2 texCoord, uint2 textureSize)
{
    return clamp(texCoord, 0, textureSize - 1);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float4> heightMap = ResourceDescriptorHeap[Resources.HeightMapTextureIndex];
    Texture2D<float4> albedo = ResourceDescriptorHeap[Resources.AlbedoTextureIndex];
    uint2 texCoords = dispatchID.xy;
    albedo[texCoords] = heightMap[texCoords];
}