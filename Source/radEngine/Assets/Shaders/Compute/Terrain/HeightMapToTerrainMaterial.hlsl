#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

ConstantBuffer<HeightToTerrainMaterialResources> Resources : register(b0);

SamplerState LinearSampler : register(s4);

uint2 ClampTexCoord(uint2 texCoord, uint2 textureSize)
{
    return clamp(texCoord, 0, textureSize - 1);
}

float3 FindNormal(float2 uv, Texture2D<float> heightMap, float totalLength)
{
    uint2 heightTextureSize;
    heightMap.GetDimensions(heightTextureSize.x, heightTextureSize.y);
    float2 texelSize = 1.0 / float2(heightTextureSize.x, heightTextureSize.y);
    
    float2 leftCoord = clamp(uv - float2(texelSize.x, 0), 0, 1);
    float2 rightCoord = clamp(uv + float2(texelSize.x, 0), 0, 1);
    float2 topCoord = clamp(uv - float2(0, texelSize.y), 0, 1);
    float2 bottomCoord = clamp(uv + float2(0, texelSize.y), 0, 1);
    
    leftCoord = (uv + leftCoord) * 0.5;
    rightCoord = (uv + rightCoord) * 0.5;
    topCoord = (uv + topCoord) * 0.5;
    bottomCoord = (uv + bottomCoord) * 0.5;
    
    float centerHeight = heightMap.Sample(LinearSampler, uv);
    float heightLeft = heightMap.Sample(LinearSampler, leftCoord);
    float heightRight = heightMap.Sample(LinearSampler, rightCoord);
    float heightTop = heightMap.Sample(LinearSampler, topCoord);
    float heightBottom = heightMap.Sample(LinearSampler, bottomCoord);
    
    //heightLeft = (centerHeight + heightLeft) * 0.5;
    //heightRight = (centerHeight + heightRight) * 0.5;
    //heightTop = (centerHeight + heightTop) * 0.5;
    //heightBottom = (centerHeight + heightBottom) * 0.5;
    
    float xDif = (heightLeft - heightRight);
    float yDif = (heightBottom - heightTop);
    
    return normalize(cross(
    normalize(float3((rightCoord - leftCoord).x * totalLength, xDif, 0)),
    normalize(float3(0, yDif, (topCoord - bottomCoord).y * totalLength))
    ));
}

[numthreads(8,8,1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
    RWTexture2D<float4> normalMap = GetBindlessResource(Resources.TerrainNormalMapTextureIndex);
    RWTexture2D<float4> albedoTex = GetBindlessResource(Resources.TerrainAlbedoTextureIndex);
    
    uint2 albedoTextureSize;
    albedoTex.GetDimensions(albedoTextureSize.x, albedoTextureSize.y);
    
    float2 texCoord = float2(dispatchID.xy) / float2(albedoTextureSize) + 0.5 / float2(albedoTextureSize);

    float heightCenter = heightMap.Sample(LinearSampler, texCoord);
    
    float3 normal = FindNormal(texCoord, heightMap, Resources.TotalLength);
    
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
    
    float slopeMinSand = 180.0;
    float slopeMaxSand = 180.0;
    
    float slopeMinGrass = 45.0;
    float slopeMaxGrass = 48.0;
    
    float slopeMinSnow = 40.0;
    float slopeMaxSnow = 42.0;
    
    float slopeMin = 0.0;
    float slopeMax = 0.0;
    
    if (heightCenter < waterHeight)
    {
        surfaceColor = sandColor;
        slopeMin = slopeMinSand;
        slopeMax = slopeMaxSand;
    }
    else if (heightCenter < waterHeight + transitionDist)
    {
        float transitionInterp = (heightCenter - waterHeight) / transitionDist;
        surfaceColor = lerp(sandColor, grassColor, transitionInterp);
        slopeMin = lerp(slopeMinSand, slopeMinGrass, transitionInterp);
        slopeMax = lerp(slopeMaxSand, slopeMaxGrass, transitionInterp);
    }
    else if (heightCenter < grassHeight)
    {
        surfaceColor = grassColor;
        slopeMin = slopeMinGrass;
        slopeMax = slopeMaxGrass;
    }
    else
    {
        float transitionInterp = saturate((heightCenter - grassHeight) / (snowHeight - grassHeight));
        surfaceColor = lerp(grassColor, snowColor, transitionInterp);
        slopeMin = lerp(slopeMinGrass, slopeMinSnow, transitionInterp);
        slopeMax = lerp(slopeMaxGrass, slopeMaxSnow, transitionInterp);
    }
    
    float interpolatedSlope = saturate((slope / PI * 180.0 - slopeMin) / (slopeMax - slopeMin));
    
    surfaceColor = lerp(surfaceColor, float3(0.25, 0.25, 0.25), interpolatedSlope);
    albedoTex[dispatchID.xy] = float4(surfaceColor, 1);
}