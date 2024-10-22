#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicCalculateOutfluxResources> Resources : register(b0);



[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.InHeightMapIndex);
    Texture2D<float> waterHeightMap = GetBindlessResource(Resources.InWaterMapIndex);
    RWTexture2D<float4> outFluxTexture = GetBindlessResource(Resources.OutFluxTextureIndex);
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    
    float curWater = waterHeightMap[dispatchID.xy];
    float curHeight = heightMap[dispatchID.xy];
    float4 curOutFlux = outFluxTexture[dispatchID.xy];
    float curTotHeight = curWater + curHeight;
    float totOutFlux = 0;
    for (int i = 0; i < 4; i++)
    {
        int2 offset = IndexToOffset4(i);
        int2 neighbor = int2(dispatchID.xy) + offset;
        if (IsInBounds(neighbor, textureSize))
        {
            float neighborTotHeight = heightMap[neighbor] + waterHeightMap[neighbor];
            float calculatedOutFlux = max(0,  curOutFlux[i] + Resources.DeltaTime * Resources.PipeCrossSection * Resources.Gravity * (curTotHeight - neighborTotHeight) / Resources.PipeLength);
            curOutFlux[i] = calculatedOutFlux;
            totOutFlux += calculatedOutFlux;
        }
    }
    
    float K = min(1.0, curWater * Resources.PipeLength * Resources.PipeLength / (totOutFlux * Resources.DeltaTime));
    
    outFluxTexture[dispatchID.xy] = curOutFlux * K;
}