#include "FullscreenVS.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"
#include "HLSLCommon.hlsli"

ConstantBuffer<LightingResources> Resources : register(b0);

SamplerState PointSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct PSIn
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

float3 LightSpaceFromWorld(float4x4 viewProjection, float3 worldPos)
{
    float4 lightSpacePos = mul(viewProjection, float4(worldPos, 1));
    lightSpacePos /= lightSpacePos.w;
    // Transform from [-1, 1] to [0, 1]
    lightSpacePos.xy = lightSpacePos.xy * 0.5 + 0.5;
    lightSpacePos.y = 1 - lightSpacePos.y;
    return lightSpacePos.xyz;
}

float shadow_offset_lookup(Texture2D<float> shadowMap, SamplerComparisonState shadowMapSampler, float3 loc, float2 offset, float bias)
{
    return shadowMap.SampleCmp(shadowMapSampler, loc.xy + offset * float2(1.0 / 1024.0, 1.0 / 1024.0), loc.z - bias).r;
}

float4 PSMain(PSIn IN) : SV_TARGET
{
    Texture2D<float4> albedoTex = GetBindlessResource(Resources.AlbedoTextureIndex);
    Texture2D<float4> normalTex = GetBindlessResource(Resources.NormalTextureIndex);
    Texture2D<float> depthMap = GetBindlessResource(Resources.DepthTextureIndex);
    
    Texture2D<float> shadowMap = GetBindlessResource(Resources.ShadowMapTextureIndex);
    SamplerComparisonState shadowMapSampler = GetBindlessSampler(Resources.ShadowMapSamplerIndex);
    ConstantBuffer<LightDataBuffer> lightData = GetBindlessResource(Resources.LightDataBufferIndex);
    ConstantBuffer<ViewTransformBuffer> viewTransform = GetBindlessResource(Resources.ViewTransformBufferIndex);
    
    float3 normal = normalTex.Sample(PointSampler, IN.TexCoord).rgb;
    float3 albedo = albedoTex.Sample(PointSampler, IN.TexCoord).rgb;
    
    float diffuse = saturate(dot(normal, -lightData.DirectionOrPosition));
    
    float halfVector = saturate(dot(normal, normalize(-lightData.DirectionOrPosition + -mul((float3x3) viewTransform.CamInverseView, float3(0, 0, 1)))));
    float specular = pow(halfVector, 32);
    
    float depth = depthMap.Sample(PointSampler, IN.TexCoord);
    float3 worldPos = WorldPosFromDepth(viewTransform.CamInverseViewProjection, IN.TexCoord, depth);
    float3 lightSpacePos = LightSpaceFromWorld(viewTransform.LightViewProjection, worldPos);
    
    bool inBounds = lightSpacePos.x > 0 && lightSpacePos.x < 1 && lightSpacePos.y > 0 && lightSpacePos.y < 1 && lightSpacePos.z > 0 && lightSpacePos.z < 1;
    
    float shadowCoeff = 1;
    if (inBounds)
    {
        float sum = 0;
        float x, y;
        static const float2 poissonDisk[16] =
        {
            float2(-0.94201624, -0.39906216),
            float2(0.94558609, -0.76890725),
            float2(-0.094184101, -0.92938870),
            float2(0.34495938, 0.29387760),
            float2(-0.91588581, 0.45771432),
            float2(-0.81544232, -0.87912464),
            float2(-0.38277543, 0.27676845),
            float2(0.97484398, 0.75648379),
            float2(0.44323325, -0.97511554),
            float2(0.53742981, -0.47373420),
            float2(-0.26496911, -0.41893023),
            float2(0.79197514, 0.19090188),
            float2(-0.24188840, 0.99706507),
            float2(-0.81409955, 0.91437590),
            float2(0.19984126, 0.78641367),
            float2(0.14383161, -0.14100790)
        };
        
        float lightSlope = dot(normal, -lightData.DirectionOrPosition);
        
        static float minBlur = 2;
        static float maxBlur = 10;
        float blur = lerp(maxBlur, minBlur, saturate(lightSlope));
        
        float biasMin = 0.0005;
        float biasMax = 0.01;
        float bias = lerp(biasMin, biasMax, saturate(lightSlope));
        
        for (int i = 0; i < 16; i++)
            sum += shadow_offset_lookup(shadowMap, shadowMapSampler, lightSpacePos, poissonDisk[i] * blur, bias);
        shadowCoeff = sum / 16.0;
    }
    
    shadowCoeff = 1 - saturate(shadowCoeff);
    
    diffuse *= shadowCoeff;
    specular *= shadowCoeff;
    //return float4(saturate(dot(normal, -lightData.DirectionOrPosition)) * float3(1, 1, 1), 1);
    //return float4(albedo, 1);
    
    
    //Texture2D<float4> reflectionTex = GetBindlessResource(Resources.ReflectionResultIndex);
    //float4 reflectionUv = reflectionTex.Sample(PointSampler, IN.TexCoord).rgba;
    //return lerp(float4((diffuse * lightData.Color + float3(0.1, 0.1, 0.1) * specular + lightData.AmbientColor) * albedo, 1),
    //float4(albedoTex.Sample(PointSampler, reflectionUv.xy).rgb, 1), reflectionUv.a);

    return float4((diffuse * lightData.Color + float3(0.4, 0.4, 0.4) * specular + lightData.AmbientColor) * albedo, 1);
}
