#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"

ConstantBuffer<rad::HeightToNormalResources> Resources : register(b0);

uint2 ClampTexCoord(uint2 texCoord, uint2 textureSize)
{
    return clamp(texCoord, 0, textureSize - 1);
}

[numthreads(8,8,1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float4> heightMap = ResourceDescriptorHeap[Resources.HeightMapTextureIndex];
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    
    uint2 texCoord = dispatchID.xy;
    float2 texelSize = 1.0 / float2(textureSize.x, textureSize.y);

    float heightCenter = heightMap[texCoord].r;
    float heightLeft = heightMap[ClampTexCoord(texCoord - uint2(1, 0), textureSize)].r;
    float heightRight = heightMap[ClampTexCoord(texCoord + uint2(1, 0), textureSize)].r;
    float heightTop = heightMap[ClampTexCoord(texCoord - uint2(0, 1), textureSize)].r;
    float heightBottom = heightMap[ClampTexCoord(texCoord + uint2(0, 1), textureSize)].r;

    float3 normal = normalize(float3(heightLeft - heightRight, heightBottom - heightTop, 2 * texelSize.x));
    Texture2D<float4> normalMap = ResourceDescriptorHeap[Resources.NormalMapTextureIndex];
    normalMap[texCoord] = float4(normal, 1);
}