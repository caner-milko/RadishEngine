#include "TerrainCommon.hlsli"

ConstantBuffer<TerrainRenderResources> Resources : register(b0);

SamplerState MipMapSampler : register(s2);
SamplerState LinearSampler : register(s4);

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


float3 FindNormal(float2 uv, Texture2D<float> heightMap, float totalLength)
{
    uint2 heightTextureSize;
    heightMap.GetDimensions(heightTextureSize.x, heightTextureSize.y);
    float2 texelSize = 1.0 / float2(heightTextureSize.x, heightTextureSize.y);
    
    float2 leftCoord = clamp(uv - float2(texelSize.x, 0), 0, 1);
    float2 rightCoord = clamp(uv + float2(texelSize.x, 0), 0, 1);
    float2 topCoord = clamp(uv - float2(0, texelSize.y), 0, 1);
    float2 bottomCoord = clamp(uv + float2(0, texelSize.y), 0, 1);
    
    leftCoord = (uv + leftCoord) * 0.5;
    rightCoord = (uv + rightCoord) * 0.5;
    topCoord = (uv + topCoord) * 0.5;
    bottomCoord = (uv + bottomCoord) * 0.5;
    
    float centerHeight = heightMap.Sample(LinearSampler, uv);
    float heightLeft = heightMap.Sample(LinearSampler, leftCoord);
    float heightRight = heightMap.Sample(LinearSampler, rightCoord);
    float heightTop = heightMap.Sample(LinearSampler, topCoord);
    float heightBottom = heightMap.Sample(LinearSampler, bottomCoord);
    
    float xDif = (heightRight - heightLeft);
    float yDif = (heightTop - heightBottom);
    
    return normalize(cross(
    normalize(float3((rightCoord - leftCoord).x * totalLength, xDif, 0)),
    normalize(float3(0, yDif, (topCoord - bottomCoord).y * totalLength))
    ));
}

[RootSignature(BindlessRootSignature)]
PSOut PSMain(VSOut IN)
{
    Texture2D<float4> albedoMap = GetBindlessResource(Resources.TerrainAlbedoTextureIndex);
    Texture2D<float4> normalMap = GetBindlessResource(Resources.TerrainNormalMapTextureIndex);
	float4 diffuseCol = albedoMap.Sample(MipMapSampler, IN.TexCoord);

    Texture2D<float> heightMap = GetBindlessResource(Resources.HeightMapTextureIndex);
	
    float heightCenter = heightMap.Sample(LinearSampler, IN.TexCoord);
    
    
    float3 normal = FindNormal(IN.TexCoord, heightMap, Resources.TotalLength);
    
    float3 sandColor = float3(194.0 / 255.0, 178.0 / 255.0, 128.0 / 255.0);
    float3 grassColor = float3(6.0 / 255.0, 77.0 / 255.0, 10.0 / 255.0);
    float3 snowColor = float3(0.9, 0.9, 0.9);
    
    float waterHeight = 0.3;
    float transitionDist = 0.05;
    float grassHeight = 0.7;
    float snowHeight = 1.0;
    
    float slope = asin(sqrt(1 - normal.y * normal.y));
    
    float3 surfaceColor;
    heightCenter /= 100.0;
    
    float slopeMinSand = 180.0;
    float slopeMaxSand = 180.0;
    
    float slopeMinGrass = 50.0;
    float slopeMaxGrass = 54.0;
    
    float slopeMinSnow = 45.0;
    float slopeMaxSnow = 50.0;
    
    float slopeMin = 0.0;
    float slopeMax = 0.0;
    
    if (heightCenter < waterHeight)
    {
        surfaceColor = sandColor;
        slopeMin = slopeMinSand;
        slopeMax = slopeMaxSand;
    }
    else if (heightCenter < waterHeight + transitionDist)
    {
        float transitionInterp = (heightCenter - waterHeight) / transitionDist;
        surfaceColor = lerp(sandColor, grassColor, transitionInterp);
        slopeMin = lerp(slopeMinSand, slopeMinGrass, transitionInterp);
        slopeMax = lerp(slopeMaxSand, slopeMaxGrass, transitionInterp);
    }
    else if (heightCenter < grassHeight)
    {
        surfaceColor = grassColor;
        slopeMin = slopeMinGrass;
        slopeMax = slopeMaxGrass;
    }
    else
    {
        float transitionInterp = saturate((heightCenter - grassHeight) / (snowHeight - grassHeight));
        surfaceColor = lerp(grassColor, snowColor, transitionInterp);
        slopeMin = lerp(slopeMinGrass, slopeMinSnow, transitionInterp);
        slopeMax = lerp(slopeMaxGrass, slopeMaxSnow, transitionInterp);
    }
    
    float interpolatedSlope = saturate((slope / PI * 180.0 - slopeMin) / (slopeMax - slopeMin));
    
    surfaceColor = lerp(surfaceColor, float3(0.25, 0.25, 0.25), interpolatedSlope);
	
    PSOut output;
	
    output.Normal = float4(normalize(mul((float3x3)Resources.Normal, normal)), 0);

    output.Albedo = float4(surfaceColor, 1.0);
	
    return output;
}