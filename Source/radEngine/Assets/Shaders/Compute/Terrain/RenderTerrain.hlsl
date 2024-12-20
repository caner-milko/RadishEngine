#include "TerrainCommon.hlsli"

ConstantBuffer<TerrainRenderResources> Resources : register(b0);

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
	float2 meshPos = float2(IN.VertexId % Resources.MeshResY, IN.VertexId / Resources.MeshResY);
	float2 texCoord = meshPos / float2(Resources.MeshResX, Resources.MeshResY);
	uint2 heightMapSize;
	heightMap.GetDimensions(heightMapSize.x, heightMapSize.y);
	uint2 heightMapTexCoord = texCoord * heightMapSize;
	float height = heightMap[heightMapTexCoord];
    float4 pos = float4((texCoord.x - 0.5) * Resources.TotalLength, height, (texCoord.y - 0.5) * Resources.TotalLength, 1.0f);
	VSOut OUT;
	OUT.Pos = mul(Resources.MVP, pos);
	OUT.TexCoord = texCoord;
	return OUT;
}

struct PSOut
{
    float4 Albedo : SV_TARGET;
    float4 Normal : SV_TARGET1;
};


[RootSignature(BindlessRootSignature)]
PSOut PSMain(VSOut IN)
{
    Texture2D<float4> albedoMap = GetBindlessResource(Resources.TerrainAlbedoTextureIndex);
    Texture2D<float4> normalMap = GetBindlessResource(Resources.TerrainNormalMapTextureIndex);
	float4 diffuseCol = albedoMap.Sample(MipMapSampler, IN.TexCoord);

    PSOut output;
    output.Albedo = diffuseCol;
	float3 normalMapVal = normalMap.Sample(MipMapSampler, IN.TexCoord).xyz * 2 - 1;
	normalMapVal = normalize(normalMapVal);
	
    float3 normal = normalize(mul((float3x3) Resources.Normal, float3(0, 1, 0)));
    float3 tangent = normalize(mul((float3x3) Resources.Normal, float3(1, 0, 0)));
    float3 bitangent = normalize(cross(normal, tangent));
	
	float3x3 TBN = float3x3(-tangent, bitangent, normal);
	
    output.Normal = float4(normalize(mul(normalMapVal, TBN)), 0);

    return output;
}