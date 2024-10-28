#include "FullscreenVS.hlsli"
#include "RenderResources.hlsli"

ConstantBuffer<BlitResources> Resources : register(b0);

SamplerState Sampler : register(s0);

[RootSignature(BindlessRootSignature)]
float4 PSMain(VSOut IN) : SV_TARGET
{
    Texture2D<float4> Source = GetBindlessResource(Resources.SourceTextureIndex);
    return Source.Sample(Sampler, IN.TexCoord);
}
