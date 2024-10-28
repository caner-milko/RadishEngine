#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicUpdateWaterVelocityResources> Resources : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float4> outFluxTexture = GetBindlessResource(Resources.InFluxTextureIndex);
    RWTexture2D<float> waterHeightMap = GetBindlessResource(Resources.OutWaterMapIndex);
    RWTexture2D<float2> velocityMap = GetBindlessResource(Resources.OutVelocityMapIndex);
    uint2 textureSize;
    waterHeightMap.GetDimensions(textureSize.x, textureSize.y);
    
    float curWater = waterHeightMap[dispatchID.xy];
    float4 curOutFlux = outFluxTexture[dispatchID.xy];
    
    float deltaVol = 0;
    float4 neighborOutflux = 0;
    for (int i = 0; i < 4; i++)
    {
        int2 offset = IndexToOffset4(i);
        int2 neighbor = int2(dispatchID.xy) + offset;
        if (IsInBounds(neighbor, textureSize))
        {
            neighborOutflux[i] = outFluxTexture[neighbor][OffsetToIndex4(-offset)];
        }
    }
    float volumeChange = 0;
    for (int i = 0; i < 4; i++)
    {
        volumeChange += neighborOutflux[i] - curOutFlux[i];
    }
    
    volumeChange *= Resources.DeltaTime;
    
    float newWater = max(curWater + volumeChange / (Resources.PipeLength * Resources.PipeLength), 0.0);
    
    float xChange = neighborOutflux[0] + curOutFlux[2] - neighborOutflux[2] - curOutFlux[0];
    
    float yChange = neighborOutflux[1] + curOutFlux[3] - neighborOutflux[3] - curOutFlux[1];
    
    waterHeightMap[dispatchID.xy] = newWater;
    float avgWater = (newWater + curWater) / 2;
    if (avgWater < 0.0001f)
    {
        velocityMap[dispatchID.xy] = float2(0, 0);
    }
    else
    {
        float invAvgL = 1 / (avgWater * Resources.PipeLength);
        velocityMap[dispatchID.xy] = 0.5f * float2(xChange, yChange) * invAvgL;
    }
}