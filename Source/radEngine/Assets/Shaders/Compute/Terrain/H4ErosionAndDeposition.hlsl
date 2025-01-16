#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicErosionAndDepositionResources> Resources : register(b0);

struct ConditionalSample
{
    float Result;
    float Distance;
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
    RWTexture2D<float> hardnessMap = GetBindlessResource(Resources.InOutHardnessMapIndex);
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
    
    left.Result = (left.Result + curHeight) * 0.5;
    right.Result = (right.Result + curHeight) * 0.5;
    top.Result = (top.Result + curHeight) * 0.5;
    bottom.Result = (bottom.Result + curHeight) * 0.5;
    
    left.Distance = left.Distance * 0.5;
    right.Distance = right.Distance * 0.5;
    top.Distance = top.Distance * 0.5;
    bottom.Distance = bottom.Distance * 0.5;
    
    float3 dhdx = normalize(float3(1.0, abs(right.Result - left.Result) / (Resources.PipeLength * (right.Distance + left.Distance)), 0));
    float3 dhdy = normalize(float3(0, abs(top.Result - bottom.Result) / ((top.Distance + bottom.Distance) * Resources.PipeLength), 1.0));
    
    float3 normal = normalize(cross(dhdx, dhdy));
    
    //normal = normalize(float3(left.Result - right.Result, 2.0 * 2.25, top.Result - bottom.Result));
    
    float sinTiltAngle = sin(acos(abs(normal.y)));
    //sinTiltAngle = abs(normal.y);
    
    sinTiltAngle = sinTiltAngle < 0.15 ? 0 : sinTiltAngle;
    
    float2 velocity = inVelocityMap[dispatchID.xy];
    
    float curWater = outWaterMap[dispatchID.xy];
    float lmax = sqrt(saturate(1 - max(0, Resources.MaximalErosionDepth - curWater) / Resources.MaximalErosionDepth));
    float hardness = hardnessMap[dispatchID.xy];
    float sedimentTransportCapacity = Resources.SedimentCapacity * max(0.15, length(velocity)) * saturate(sinTiltAngle) * lmax;
    float sediment = outSedimentMap[dispatchID.xy];
    
    if (sediment < sedimentTransportCapacity)
    {
        float mod = Resources.DeltaTime * (1 - hardness) * Resources.SoilSuspensionRate * (sedimentTransportCapacity - sediment);
        outHeightMap[dispatchID.xy] = curHeight - mod;
        outSedimentMap[dispatchID.xy] += mod;
        outWaterMap[dispatchID.xy] += mod;
        hardnessMap[dispatchID.xy] = clamp(hardness + Resources.SoilHardeningRate * mod, 0.0, Resources.MaximumHardness);
    }
    else
    {
        float mod = Resources.DeltaTime * Resources.SedimentDepositionRate * (sediment - sedimentTransportCapacity);
        mod = min(mod, curWater);
        outHeightMap[dispatchID.xy] = curHeight + mod;
        outSedimentMap[dispatchID.xy] -= mod;
        outWaterMap[dispatchID.xy] -= mod;
        
        hardnessMap[dispatchID.xy] = clamp(hardness - Resources.SoilHardeningRate * mod, 0.0, Resources.MaximumHardness);
    }
}