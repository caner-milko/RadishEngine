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

SamplerState Sampler : register(s0);

struct PSIn
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSIn IN) : SV_TARGET
{
    
    float3 normal = Normal.Sample(Sampler, IN.TexCoord).rgb;
    float3 albedo = Albedo.Sample(Sampler, IN.TexCoord).rgb;
    
    float diffuse = saturate(dot(normal, LightCB.DirectionOrPosition));
    
    float halfVector = saturate(dot(normal, normalize(LightCB.DirectionOrPosition + float3(0, 0, 1))));
    float specular = pow(halfVector, 32);
    
    diffuse *= LightCB.Intensity;
    
    return float4((diffuse * LightCB.Color + float3(0.1, 0.1, 0.1) * specular + LightCB.AmbientColor) * albedo, 1);
}
