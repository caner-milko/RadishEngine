struct LightData
{
    float3 DirectionOrPosition;
    float Padding;
    float3 Color;
    float Intensity;
    float3 AmbientColor;
    float Padding2;
};
ConstantBuffer<LightData> LightCB : register(b0);

Texture2D Albedo : register(t0);
Texture2D Normal : register(t1);
Texture2D Depth : register(t2);

struct TransformationMatricesCB
{
    matrix LightViewProjection;
    matrix CamInverseView;
    matrix CamInverseProjection;
};

ConstantBuffer<TransformationMatricesCB> TransformationMatrices : register(b1);
Texture2D ShadowMap : register(t3);

SamplerState PointSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct PSIn
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

float3 WorldPosFromDepth(float2 texCoord, float depth)
{
    float2 screenPos = texCoord * 2 - 1;
    screenPos.y = -screenPos.y;
    float4 clipPos = float4(screenPos, depth, 1);
    float4 viewPos = mul(TransformationMatrices.CamInverseProjection, clipPos);
    viewPos /= viewPos.w;
    float4 worldPos = mul(TransformationMatrices.CamInverseView, viewPos);
    return worldPos.xyz;
}

float3 LightSpaceFromWorld(float3 worldPos)
{
    float4 lightSpacePos = mul(TransformationMatrices.LightViewProjection, float4(worldPos, 1));
    lightSpacePos /= lightSpacePos.w;
    // Transform from [-1, 1] to [0, 1]
    lightSpacePos.xy = lightSpacePos.xy * 0.5 + 0.5;
    lightSpacePos.y = 1 - lightSpacePos.y;
    return lightSpacePos.xyz;
}

float shadow_offset_lookup(float3 loc, float2 offset)
{
    return ShadowMap.SampleCmp(ShadowSampler, loc.xy + offset * float2(1.0 / 1024.0, 1.0 / 1024.0), loc.z - 1e-3).r;
}

float4 main(PSIn IN) : SV_TARGET
{
    
    float3 normal = Normal.Sample(PointSampler, IN.TexCoord).rgb;
    float3 albedo = Albedo.Sample(PointSampler, IN.TexCoord).rgb;
    
    float diffuse = saturate(dot(normal, -LightCB.DirectionOrPosition));
    
    float halfVector = saturate(dot(normal, normalize(-LightCB.DirectionOrPosition + float3(0, 0, 1))));
    float specular = pow(halfVector, 32);
    
    diffuse *= LightCB.Intensity;
    
    float depth = Depth.Sample(PointSampler, IN.TexCoord).r;
    float3 worldPos = WorldPosFromDepth(IN.TexCoord, depth);
    float3 lightSpacePos = LightSpaceFromWorld(worldPos);
    
    bool inBounds = lightSpacePos.x > 0 && lightSpacePos.x < 1 && lightSpacePos.y > 0 && lightSpacePos.y < 1 && lightSpacePos.z > 0 && lightSpacePos.z < 1;
    
    float shadowCoeff = 1;
    if(inBounds)
    {
        float sum = 0;
        float x, y;
        for (y = -1.5; y <= 1.5; y += 1.0)
            for (x = -1.5; x <= 1.5; x += 1.0)
                sum += shadow_offset_lookup(lightSpacePos, float2(x, y));
        shadowCoeff = sum / 16.0;
    }
    
    shadowCoeff = 1 - saturate(shadowCoeff);
    
    diffuse *= shadowCoeff;
    specular *= shadowCoeff;
    
    
    return float4((diffuse * LightCB.Color + float3(0.1, 0.1, 0.1) * specular + LightCB.AmbientColor) * albedo, 1);
}
