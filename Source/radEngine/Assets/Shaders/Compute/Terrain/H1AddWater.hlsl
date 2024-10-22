#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicAddWaterResources> Resources : register(b0);


float random(float2 v)
{
    return frac(sin(dot(v.xy,
      float2(12.9898, 78.233))) *
      43758.5453123);
}

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    RWTexture2D<float> waterMap = GetBindlessResource(Resources.WaterMapIndex);
    uint2 textureSize;
    waterMap.GetDimensions(textureSize.x, textureSize.y);
    
    float multiplier = 10.0f;
    
    float sec = multiplier * Resources.Iteration * Resources.DeltaTime;
    
    float toAdd = 0.0;
    
    if(frac(sec) < Resources.DeltaTime / multiplier)
    {
        float rainDropRadius = 0.05f;
        float dropX = random(float2(sec, 0));
        float dropY = random(float2(sec, 1));
        float minDropRadius = rainDropRadius * 0.2f;
        float maxDropRadius = rainDropRadius * 1.0f;
        float2 dropCenter = float2(dropX, dropY);
        float dropRadius = lerp(minDropRadius, maxDropRadius, random(float2(sec, 2)));
        
        float minStrength = 0.4f;
        float maxStrength = 2.0f;
        float strength = lerp(minStrength, maxStrength, pow(random(float2(sec, 3)), 2.0));
        
        float distance = length(dropCenter - (dispatchID.xy) / float2(textureSize));
        
        if (distance < dropRadius)
        {
            toAdd += strength;
        }
    }
    
    float rain = Resources.RainRate * Resources.DeltaTime;
    
    waterMap[dispatchID.xy] = waterMap[dispatchID.xy] + toAdd + rain * 0.0;
}