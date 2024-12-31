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
    float3 WorldPos : WORLDPOS;
    float2 TexCoord : TEXCOORD;
    float2 ScreenUv : TEXCOORD1;
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
    OUT.WorldPos = mul(Resources.ModelMatrix, pos).xyz;
    OUT.ScreenUv = float2(OUT.Pos.x, -OUT.Pos.y) / OUT.Pos.w * 0.5 + 0.5;
	return OUT;
}

struct PSOut
{
    float4 Color : SV_TARGET;
};


[RootSignature(BindlessRootSignature)]
PSOut PSMain(VSOut IN)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
    Texture2D<float> waterHeightMap = GetBindlessResource(Resources.WaterHeightMapTextureIndex);
    
    float waterHeight = waterHeightMap.Sample(MipMapSampler, IN.TexCoord);
    if(waterHeight < 0.01)
        discard;
    
    
    Texture2D<float4> albedoMap = GetBindlessResource(Resources.WaterAlbedoTextureIndex);
    Texture2D<float4> normalMap = GetBindlessResource(Resources.WaterNormalMapTextureIndex);
	float4 diffuseCol = albedoMap.Sample(MipMapSampler, IN.TexCoord);

    Texture2D<float4> reflectionResult = GetBindlessResource(Resources.ReflectionResultTextureIndex);
    Texture2D<float4> refractionResult = GetBindlessResource(Resources.RefractionResultTextureIndex);
    Texture2D<float4> colorTex = GetBindlessResource(Resources.ColorTextureIndex);
    
    float4 reflectionUvVis = reflectionResult.Sample(linearSampler, IN.ScreenUv);
    float4 refractionUvVis = refractionResult.Sample(linearSampler, IN.ScreenUv);
    
    // Sky color
    float3 skyColor = float3(0.53, 0.80, .92);
    float3 reflectionColor = skyColor;
    if (reflectionUvVis.a > 0)
        reflectionColor = lerp(reflectionColor, colorTex.Sample(linearSampler, reflectionUvVis.xy).rgb, reflectionUvVis.a);
    
    float3 refractionColor = colorTex.Sample(linearSampler, IN.ScreenUv).rgb;
    if (refractionUvVis.a > 0)                                 
        refractionColor = lerp(refractionColor, colorTex.Sample(linearSampler, refractionUvVis.xy).rgb, refractionUvVis.a);
    
    float3 normalMapVal = normalMap.Sample(MipMapSampler, IN.TexCoord).xyz * 2 - 1;
    normalMapVal = normalize(normalMapVal);
	
    float3 normal = normalize(mul((float3x3) Resources.Normal, float3(0, 1, 0)));
    float3 tangent = normalize(mul((float3x3) Resources.Normal, float3(-1, 0, 0)));
    float3 bitangent = normalize(cross(normal, tangent));
	
    float3x3 TBN = float3x3(tangent, bitangent, normal);
	
    float3 waterSurfaceNormal = normalize(mul(TBN, normalMapVal));
    
    float3 viewDir = normalize(IN.WorldPos - Resources.ViewPos.xyz);
    
    float fresnel = 1 - max(dot(-viewDir, waterSurfaceNormal), 0);
    
    fresnel = sqrt(fresnel);
    
    PSOut output;
    output.Color = float4(lerp(refractionColor, reflectionColor, fresnel), 1);
    return output;
}