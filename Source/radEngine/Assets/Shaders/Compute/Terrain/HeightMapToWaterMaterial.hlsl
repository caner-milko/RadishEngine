#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

ConstantBuffer<HeightToWaterMaterialResources> Resources : register(b0);

SamplerState LinearSampler : register(s4);

uint2 ClampTexCoord(uint2 texCoord, uint2 textureSize)
{
    return clamp(texCoord, 0, textureSize - 1);
}

float3 FindNormal(float2 uv, Texture2D<float> heightMap, Texture2D<float> waterMap, float cellSize)
{
    uint2 heightTextureSize;
    heightMap.GetDimensions(heightTextureSize.x, heightTextureSize.y);
    float2 texelSize = 1.0 / float2(heightTextureSize.x, heightTextureSize.y);
    
    float2 leftCoord = clamp(uv - float2(texelSize.x, 0), 0, 1);
    float2 rightCoord = clamp(uv + float2(texelSize.x, 0), 0, 1);
    float2 topCoord = clamp(uv - float2(0, texelSize.y), 0, 1);
    float2 bottomCoord = clamp(uv + float2(0, texelSize.y), 0, 1);
    
    float heightLeft = heightMap.Sample(LinearSampler, leftCoord) + waterMap.Sample(LinearSampler, leftCoord);
    float heightRight = heightMap.Sample(LinearSampler, rightCoord) + waterMap.Sample(LinearSampler, rightCoord);
    float heightTop = heightMap.Sample(LinearSampler, topCoord) + waterMap.Sample(LinearSampler, topCoord);
    float heightBottom = heightMap.Sample(LinearSampler, bottomCoord) + waterMap.Sample(LinearSampler, bottomCoord);
    
    float xDif = (heightLeft - heightRight);
    float yDif = (heightBottom - heightTop);
    
    return normalize(cross(
    normalize(float3((rightCoord - leftCoord).x / texelSize.x * cellSize, xDif, 0)),
    normalize(float3(0, yDif, (topCoord - bottomCoord).y / texelSize.y * cellSize))
    ));
}

[numthreads(8,8,1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
    Texture2D<float> waterMap = GetBindlessResource(Resources.WaterHeightMapTextureIndex);
    Texture2D<float> sedimentMap = GetBindlessResource(Resources.SedimentMapTextureIndex);
    RWTexture2D<float4> waterAlbedoTex = GetBindlessResource(Resources.WaterAlbedoTextureIndex);
    RWTexture2D<float4> waterNormalMap = GetBindlessResource(Resources.WaterNormalMapTextureIndex);
    
    uint2 albedoTextureSize;
    waterAlbedoTex.GetDimensions(albedoTextureSize.x, albedoTextureSize.y);
    
    float2 texCoord = float2(dispatchID.xy) / float2(albedoTextureSize) + 0.5 / float2(albedoTextureSize);

    float heightCenter = heightMap.Sample(LinearSampler, texCoord);
    
    float water = waterMap.Sample(LinearSampler, texCoord);
    
    float3 waterNormal = FindNormal(texCoord, heightMap, waterMap, Resources.CellSize);
    waterNormal = waterNormal * 0.5 + 0.5;
    waterNormalMap[dispatchID.xy] = float4(waterNormal, 0);
    
    float sediment = sedimentMap.Sample(LinearSampler, texCoord);
    float3 waterCol = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), saturate(sediment * 1.0));
    waterAlbedoTex[dispatchID.xy] = float4(waterCol, lerp(0, 1, water > 0.05));
}