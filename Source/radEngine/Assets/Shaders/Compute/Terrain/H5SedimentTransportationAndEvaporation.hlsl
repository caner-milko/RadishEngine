#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicSedimentTransportationAndEvaporationResources> Resources : register(b0);
    
SamplerState linearSampler : register(s6);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    // Sediment transportation
    Texture2D<float2> inVelocityMap = GetBindlessResource(Resources.InVelocityMapIndex);
    Texture2D<float> inOldSedimentMap = GetBindlessResource(Resources.InOldSedimentMapIndex);
    RWTexture2D<float> outSedimentMap = GetBindlessResource(Resources.OutSedimentMapIndex);
    uint2 textureDimensions;
    inVelocityMap.GetDimensions(textureDimensions.x, textureDimensions.y);
    
    
    float2 velocity = inVelocityMap[dispatchID.xy];
    float2 pos = dispatchID.xy / float2(textureDimensions);
    float2 oldPos = pos - velocity / float(textureDimensions.x) * Resources.DeltaTime;
    float oldSediment = inOldSedimentMap.Sample(linearSampler, oldPos);
    //outSedimentMap[dispatchID.xy] = oldSediment;
    outSedimentMap[dispatchID.xy] = inOldSedimentMap[dispatchID.xy];
    // Evaporation
    RWTexture2D<float> waterMap = GetBindlessResource(Resources.InOutWaterMapIndex);
    waterMap[dispatchID.xy] *= 1 - Resources.EvaporationRate * Resources.DeltaTime;
}