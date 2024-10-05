#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

ConstantBuffer<rad::ThermalErosionResources> Resources : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> inHeightMap = ResourceDescriptorHeap[Resources.HeightMapIndex];
    Texture2D<float> outHeightMap = ResourceDescriptorHeap[Resources.OutHeightMapIndex];
    
}