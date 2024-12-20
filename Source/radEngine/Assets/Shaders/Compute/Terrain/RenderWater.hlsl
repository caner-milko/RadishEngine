#include "TerrainCommon.hlsli"

ConstantBuffer<WaterRenderResources> Resources : register(b0);

SamplerState MipMapSampler : register(s2);
SamplerState linearSampler : register(s4);

struct VSIn
{
	uint VertexId : SV_VertexID;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

[RootSignature(BindlessRootSignature)]
VSOut VSMain(VSIn IN)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
    Texture2D<float> waterHeightMap = GetBindlessResource(Resources.WaterHeightMapTextureIndex);
	float2 meshPos = float2(IN.VertexId % Resources.MeshResY, IN.VertexId / Resources.MeshResY);
	float2 texCoord = meshPos / float2(Resources.MeshResX, Resources.MeshResY);
	uint2 heightMapSize;
	heightMap.GetDimensions(heightMapSize.x, heightMapSize.y);
	uint2 heightMapTexCoord = texCoord * heightMapSize;
    float height = heightMap[heightMapTexCoord];
    float waterHeight = waterHeightMap[heightMapTexCoord];
    float4 pos = float4((texCoord.x - 0.5) * Resources.TotalLength, height + waterHeight, (texCoord.y - 0.5) * Resources.TotalLength, 1.0f);
	VSOut OUT;
	OUT.Pos = mul(Resources.MVP, pos);
	OUT.TexCoord = texCoord;
	return OUT;
}

struct PSOut
{
    float4 Color : SV_TARGET;
};


[RootSignature(BindlessRootSignature)]
PSOut PSMain(VSOut IN)
{
    Texture2D<float4> albedoMap = GetBindlessResource(Resources.WaterAlbedoTextureIndex);
    Texture2D<float4> normalMap = GetBindlessResource(Resources.WaterNormalMapTextureIndex);
	float4 diffuseCol = albedoMap.Sample(MipMapSampler, IN.TexCoord);

    PSOut output;
    output.Color = float4(diffuseCol.rgb, diffuseCol.a * 0.5);
    return output;
}