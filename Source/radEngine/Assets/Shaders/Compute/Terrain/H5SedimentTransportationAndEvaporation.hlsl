#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicSedimentTransportationAndEvaporationResources> Resources : register(b0);
    
SamplerState linearSampler : register(s4);

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
    float texelSize = 1.0 / float(textureDimensions.x);
    
    float2 velocity = inVelocityMap[dispatchID.xy];
    float2 pos = dispatchID.xy / float2(textureDimensions);
    float2 oldPos = saturate(pos + texelSize * 0.5 - velocity * Resources.DeltaTime * texelSize * Resources.PipeLength);
    float oldSediment = inOldSedimentMap.Sample(linearSampler, oldPos );
    outSedimentMap[dispatchID.xy] = oldSediment;
    // Evaporation
    RWTexture2D<float> waterMap = GetBindlessResource(Resources.InOutWaterMapIndex);
    waterMap[dispatchID.xy] *= 1 - Resources.EvaporationRate * Resources.DeltaTime;
}