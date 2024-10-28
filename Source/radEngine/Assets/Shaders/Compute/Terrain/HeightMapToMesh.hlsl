#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

ConstantBuffer<HeightToMeshResources> Resources : register(b0);

SamplerState LinearSampler : register(s4);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
    Texture2D<float> waterMap = GetBindlessResource(Resources.WaterHeightMapTextureIndex);
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    
    uint2 meshCoord = dispatchID.xy;
    float2 texelSize = 1.0 / float2(textureSize.x, textureSize.y);
    float2 uv = float2(meshCoord) / float2(Resources.MeshResX, Resources.MeshResY);
    
    float heightCur = heightMap.Sample(LinearSampler, uv);
    
    RWStructuredBuffer<Vertex> vertexBuf = GetBindlessResource(Resources.TerrainVertexBufferIndex);
    RWStructuredBuffer<Vertex> waterVertexBuf = GetBindlessResource(Resources.WaterVertexBufferIndex);
    
    Vertex vtx;
    vtx.Position = float3(uv.x * Resources.CellSize / texelSize.x, heightCur, uv.y * Resources.CellSize / texelSize.y);
    vtx.Normal = float3(0, 1, 0);
    vtx.TexCoord = uv;
    vtx.Tangent = float3(1, 0, 0);
    vertexBuf[meshCoord.y * Resources.MeshResX + meshCoord.x] = vtx;
    
    float water = waterMap.Sample(LinearSampler, uv);
    vtx.Position = float3(uv.x * Resources.CellSize / texelSize.x, heightCur + water, uv.y * Resources.CellSize / texelSize.y);
    waterVertexBuf[meshCoord.y * Resources.MeshResX + meshCoord.x] = vtx;
}