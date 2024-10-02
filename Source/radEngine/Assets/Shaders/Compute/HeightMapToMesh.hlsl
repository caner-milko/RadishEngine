#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

ConstantBuffer<rad::HeightToMeshResources> Resources : register(b0);

SamplerState LinearSampler : register(s6);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float4> heightMap = ResourceDescriptorHeap[Resources.HeightMapTextureIndex];
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    
    uint2 meshCoord = dispatchID.xy;
    float2 texelSize = 1.0 / float2(textureSize.x, textureSize.y);
    float2 uv = float2(meshCoord) / float2(Resources.MeshResX, Resources.MeshResY);
    
    float heightCur = heightMap.Sample(LinearSampler, uv).r;
    
    RWStructuredBuffer<rad::Vertex> vertexBuf = ResourceDescriptorHeap[Resources.VertexBufferIndex];
    
    rad::Vertex vtx;
    vtx.Position = float3(uv.x * 2.0, heightCur, uv.y * 2.0);
    vtx.Normal = float3(0, 1, 0);
    vtx.TexCoord = uv;
    vtx.Tangent = float3(1, 0, 0);
    vertexBuf[meshCoord.y * Resources.MeshResX + meshCoord.x] = vtx;
}