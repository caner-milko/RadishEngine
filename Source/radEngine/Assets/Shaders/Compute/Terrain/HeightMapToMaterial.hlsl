#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

ConstantBuffer<HeightToMaterialResources> Resources : register(b0);

SamplerState LinearSampler : register(s4);

uint2 ClampTexCoord(uint2 texCoord, uint2 textureSize)
{
    return clamp(texCoord, 0, textureSize - 1);
}

float3 FindNormal(float2 uv, Texture2D<float> heightMap, Texture2D<float> waterMap, bool useWaterMap, float cellSize)
{
    uint2 heightTextureSize;
    heightMap.GetDimensions(heightTextureSize.x, heightTextureSize.y);
    float2 texelSize = 1.0 / float2(heightTextureSize.x, heightTextureSize.y);
    
    float2 leftCoord = clamp(uv - float2(texelSize.x, 0), 0, 1);
    float2 rightCoord = clamp(uv + float2(texelSize.x, 0), 0, 1);
    float2 topCoord = clamp(uv - float2(0, texelSize.y), 0, 1);
    float2 bottomCoord = clamp(uv + float2(0, texelSize.y), 0, 1);
    
    float heightLeft = heightMap.Sample(LinearSampler, leftCoord);
    float heightRight = heightMap.Sample(LinearSampler, rightCoord);
    float heightTop = heightMap.Sample(LinearSampler, topCoord);
    float heightBottom = heightMap.Sample(LinearSampler, bottomCoord);

    if(useWaterMap)
    {
        float waterLeft = waterMap.Sample(LinearSampler, leftCoord);
        float waterRight = waterMap.Sample(LinearSampler, rightCoord);
        float waterTop = waterMap.Sample(LinearSampler, topCoord);
        float waterBottom = waterMap.Sample(LinearSampler, bottomCoord);
        heightLeft += waterLeft;
        heightRight += waterRight;
        heightTop += waterTop;
        heightBottom += waterBottom;
    }
    
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
    RWTexture2D<float4> normalMap = GetBindlessResource(Resources.TerrainNormalMapTextureIndex);
    RWTexture2D<float4> albedoTex = GetBindlessResource(Resources.TerrainAlbedoTextureIndex);
    RWTexture2D<float4> waterAlbedoTex = GetBindlessResource(Resources.WaterAlbedoTextureIndex);
    RWTexture2D<float4> waterNormalMap = GetBindlessResource(Resources.WaterNormalMapTextureIndex);
    
    uint2 albedoTextureSize;
    albedoTex.GetDimensions(albedoTextureSize.x, albedoTextureSize.y);
    
    float2 texCoord = float2(dispatchID.xy) / float2(albedoTextureSize) + 0.5 / float2(albedoTextureSize);

    float heightCenter = heightMap.Sample(LinearSampler, texCoord);
    
    float3 normal = FindNormal(texCoord, heightMap, waterMap, false, Resources.CellSize);
    
    float3 mapVal = float3(normal.xzy);
    mapVal = mapVal * 0.5 + 0.5;
    normalMap[dispatchID.xy] = float4(mapVal, 0);
    
    float3 sandColor = float3(194.0/255.0, 178.0/255.0, 128.0/255.0);
    float3 grassColor = float3(6.0/255.0, 77.0/255.0, 10.0/255.0);
    float3 snowColor = float3(0.9, 0.9, 0.9);
    
    float waterHeight = 0.3;
    float transitionDist = 0.05;
    float grassHeight = 0.7;
    float snowHeight = 1.0;
    
    float slope = asin(sqrt(1 - normal.y * normal.y));
    
    float3 surfaceColor;
    heightCenter /= 100.0;
    if (heightCenter < waterHeight)
    {
        surfaceColor = sandColor;
    }
    else if (heightCenter < waterHeight + transitionDist)
    {
        surfaceColor = lerp(sandColor, grassColor, (heightCenter - waterHeight) / transitionDist);
    }
    else if (heightCenter < grassHeight)
    {
        surfaceColor = grassColor;
    }
    else
    {
        surfaceColor = lerp(grassColor, snowColor, (heightCenter - grassHeight) / (snowHeight - grassHeight));
    }
    float sediment = sedimentMap.Sample(LinearSampler, texCoord);
    float water = waterMap.Sample(LinearSampler, texCoord);
    
    float slopeMin = 50.0 / 180.0;
    surfaceColor = lerp(surfaceColor, float3(0.25, 0.25, 0.25), max(0, slope / (PI / 2) - slopeMin) / (1 - slopeMin));
    albedoTex[dispatchID.xy] = float4(surfaceColor, 1);
    //albedoTex[dispatchID.xy] = float4(heightCenter, heightCenter, heightCenter, 1);
    
    float3 waterNormal = FindNormal(texCoord, heightMap, waterMap, true, Resources.CellSize);
    mapVal = float3(waterNormal.xzy);
    mapVal = mapVal * 0.5 + 0.5;
    waterNormalMap[dispatchID.xy] = float4(mapVal, 0);
    
    
    
    float3 waterCol = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), saturate(sediment * 1.0));
    
    waterAlbedoTex[dispatchID.xy] = float4(waterCol, lerp(0, 1, water > 0.05));
}