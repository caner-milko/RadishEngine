#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

ConstantBuffer<rad::HeightToMeshResources> Resources : register(b0);

SamplerState LinearSampler : register(s2);

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    //Texture2D<float4> heightMap = ResourceDescriptorHeap[Resources.HeightMapTextureIndex];
    //uint2 textureSize;
    //heightMap.GetDimensions(textureSize.x, textureSize.y);
    //
    //uint2 meshCoord = dispatchID.xy;
    //float2 texelSize = 1.0 / float2(textureSize.x, textureSize.y);
    //float2 uv = float2(meshCoord) / float2(Resources.MeshResX, Resources.MeshResY);
    //
    //
    //float heightCur = heightMap.Sample(LinearSampler, uv).r;
    //
    //float heightLeft = heightMap.Sample(LinearSampler, uv - float2(texelSize.x, 0)).r;
    //float heightRight = heightMap.Sample(LinearSampler, uv + float2(texelSize.x, 0)).r;
    //float heightUp = heightMap.Sample(LinearSampler, uv - float2(0, texelSize.y)).r;
    //float heightDown = heightMap.Sample(LinearSampler, uv + float2(0, texelSize.y)).r;
    //
    //float3 normal = normalize(float3((heightLeft - heightRight) / (2 * texelSize.x), (heightDown - heightUp) / (2 * texelSize.y), 2));
    //float3 tangent = float3(0, (heightUp - heightDown) / (2*texelSize.y), 2);
    //
    //RWStructuredBuffer<rad::Vertex> vertexBuf = ResourceDescriptorHeap[Resources.VertexBufferIndex];
    //
    //rad::Vertex vtx;
    //vtx.Position = float3(uv.x, heightCur, uv.y);
    //vtx.Normal = normal;
    //vtx.TexCoord = uv;
    //vtx.Tangent = tangent;
    //for (int i = 0; i < 100; i++)
    //{
    //    vtx.Position = float3(1.0f, i, 1.0f);
    //    vertexBuf[i] = vtx;
    //}
}