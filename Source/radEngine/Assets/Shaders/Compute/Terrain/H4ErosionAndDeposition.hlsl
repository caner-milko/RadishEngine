#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicErosionAndDepositionResources> Resources : register(b0);

struct ConditionalSample
{
    float Result;
    uint Distance;
};

ConditionalSample SampleDirection(Texture2D<float> heightMap, uint2 textureSize, uint2 pos, int2 offset)
{
    int2 realOffset = int2(pos) + offset;
    if (realOffset.x < 0 || realOffset.y < 0 || realOffset.x >= textureSize.x || realOffset.y >= textureSize.y)
    {
        realOffset = int2(pos);
    }
    ConditionalSample sample;
    sample.Result = heightMap[realOffset];
    sample.Distance = dot(realOffset - int2(pos), realOffset - int2(pos));
    return sample;
}
    
[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float2> inVelocityMap = GetBindlessResource(Resources.InVelocityMapIndex);
    Texture2D<float> inOldHeightMap = GetBindlessResource(Resources.InOldHeightMapIndex);
    RWTexture2D<float> softnessMap = GetBindlessResource(Resources.InOutSoftnessMapIndex);
    RWTexture2D<float> outHeightMap = GetBindlessResource(Resources.OutHeightMapIndex);
    RWTexture2D<float> outWaterMap = GetBindlessResource(Resources.OutWaterMapIndex);
    RWTexture2D<float> outSedimentMap = GetBindlessResource(Resources.OutSedimentMapIndex);
    uint2 textureSize;
    inOldHeightMap.GetDimensions(textureSize.x, textureSize.y);
    
    float curHeight = inOldHeightMap[dispatchID.xy];
    
    ConditionalSample left = SampleDirection(inOldHeightMap, textureSize, dispatchID.xy, int2(-1, 0));
    ConditionalSample right = SampleDirection(inOldHeightMap, textureSize, dispatchID.xy, int2(1, 0));
    ConditionalSample top = SampleDirection(inOldHeightMap, textureSize, dispatchID.xy, int2(0, 1));
    ConditionalSample bottom = SampleDirection(inOldHeightMap, textureSize, dispatchID.xy, int2(0, -1));
    
    float3 dhdx = normalize(float3((right.Distance + left.Distance) * Resources.PipeLength, (right.Result - left.Result), 0));
    float3 dhdy = normalize(float3(0, (top.Result - bottom.Result), (top.Distance + bottom.Distance) * Resources.PipeLength));
    
    float3 normal = normalize(cross(dhdx, dhdy));
    
    //normal = normalize(float3(left.Result - right.Result, 2.0 * 2.25, top.Result - bottom.Result));
    
    float sinTiltAngle = abs(sqrt(1.0 - normal.y * normal.y));
    //sinTiltAngle = abs(normal.y);
    
    float2 velocity = inVelocityMap[dispatchID.xy];
    
    float curWater = outWaterMap[dispatchID.xy];
    float lmax = saturate(1 - max(0, Resources.MaximalErosionDepth - curWater) / Resources.MaximalErosionDepth);
    float hardness = softnessMap[dispatchID.xy];
    float sedimentTransportCapacity = Resources.SedimentCapacity * length(velocity) * max(sinTiltAngle, 0.05)  * lmax;
    //sedimentTransportCapacity = min(sedimentTransportCapacity, Resources.SedimentCapacity);
    float sediment = outSedimentMap[dispatchID.xy];
    
    if (sediment < sedimentTransportCapacity)
    {
        float mod = min(hardness * Resources.DeltaTime * Resources.SoilSuspensionRate * (sedimentTransportCapacity - sediment), curHeight);
        outHeightMap[dispatchID.xy] = curHeight - mod;
        outSedimentMap[dispatchID.xy] += mod;
        outWaterMap[dispatchID.xy] += mod;
        softnessMap[dispatchID.xy] = max(Resources.MinimumSoftness, hardness - mod * Resources.SoilHardeningRate);
    }
    else
    {
        float mod = min(Resources.DeltaTime * Resources.SedimentDepositionRate * (sediment - sedimentTransportCapacity), curWater);
        outHeightMap[dispatchID.xy] = curHeight + mod;
        outSedimentMap[dispatchID.xy] -= mod;
        outWaterMap[dispatchID.xy] -= mod;
        softnessMap[dispatchID.xy] = min(1.0, hardness + mod * Resources.SoilSofteningRate);
    }
}