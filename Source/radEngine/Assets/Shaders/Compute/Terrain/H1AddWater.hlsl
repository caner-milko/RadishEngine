#include "TerrainCommon.hlsli"

ConstantBuffer<HydrolicAddWaterResources> Resources : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    RWTexture2D<float> waterMap = GetBindlessResource(Resources.WaterMapIndex);
    waterMap[dispatchID.xy] = waterMap[dispatchID.xy] + Resources.RainRate * Resources.DeltaTime;
}