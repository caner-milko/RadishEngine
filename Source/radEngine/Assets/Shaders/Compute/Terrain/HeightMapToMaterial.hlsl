#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

ConstantBuffer<HeightToMaterialResources> Resources : register(b0);

SamplerState LinearSampler : register(s6);

uint2 ClampTexCoord(uint2 texCoord, uint2 textureSize)
{
    return clamp(texCoord, 0, textureSize - 1);
}

[numthreads(8,8,1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
    RWTexture2D<float4> normalMap = GetBindlessResource(Resources.NormalMapTextureIndex);
    RWTexture2D<float4> albedoTex = GetBindlessResource(Resources.AlbedoTextureIndex);
    uint2 heightTextureSize;
    heightMap.GetDimensions(heightTextureSize.x, heightTextureSize.y);
    float2 texelSize = 1.0 / float2(heightTextureSize.x, heightTextureSize.y);
    
    uint2 albedoTextureSize;
    albedoTex.GetDimensions(albedoTextureSize.x, albedoTextureSize.y);
    
    float2 texCoord = float2(dispatchID.xy) / float2(albedoTextureSize);

    float heightCenter = heightMap.Sample(LinearSampler, texCoord).r;
    float heightLeft = heightMap.Sample(LinearSampler, texCoord - float2(texelSize.x, 0));
    float heightRight = heightMap.Sample(LinearSampler, texCoord + float2(texelSize.x, 0));
    float heightTop = heightMap.Sample(LinearSampler, texCoord - float2(0, texelSize.y));
    float heightBottom = heightMap.Sample(LinearSampler, texCoord + float2(0, texelSize.y));

    float xDif = (heightLeft - heightRight) / (2 * texelSize.x);
    float yDif = (heightBottom - heightTop) / (2 * texelSize.y);
    
    float3 normal = normalize(float3(xDif, 2, yDif));
    float3 mapVal = float3(normal.xzy);
    mapVal.x *= -1;
    mapVal.xy /= 3;
    mapVal = mapVal * 0.5 + 0.5;
    normalMap[dispatchID.xy] = float4(mapVal, 0);
    
    float3 waterColor = float3(0.0, 0.0, 1.0);
    float3 grassColor = float3(0.0, 1.0, 0.0);
    float3 snowColor = float3(1.0, 1.0, 1.0);
    
    float waterHeight = 0.3;
    float transitionDist = 0.05;
    float grassHeight = 0.7;
    float snowHeight = 1.0;
    
    float3 surfaceColor;
    if(heightCenter < waterHeight)
    {
        surfaceColor = waterColor;
    }
    else if (heightCenter < waterHeight + transitionDist)
    {
        surfaceColor = lerp(waterColor, grassColor, (heightCenter - waterHeight) / transitionDist);
    }
    else if (heightCenter < grassHeight)
    {
        surfaceColor = grassColor;
    }
    else
    {
        surfaceColor = lerp(grassColor, snowColor, (heightCenter - grassHeight) / (snowHeight - grassHeight));
    }
    
    albedoTex[dispatchID.xy] = float4(surfaceColor, 1);
    
}