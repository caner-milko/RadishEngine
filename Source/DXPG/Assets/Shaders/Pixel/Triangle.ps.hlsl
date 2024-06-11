struct Material
{
    float4 Diffuse;
    int UseDiffuseTexture;
    int UseAlphaMask;
};
ConstantBuffer<Material> MaterialCB : register(b1);

struct LightData
{
    float3 DirectionOrPosition;
    float Padding;
    float3 Color;
    float Intensity;
    float3 AmbientColor;
    float Padding2;
};
ConstantBuffer<LightData> LightCB : register(b2);

Texture2D Diffuse : register(t3);
Texture2D Alpha : register(t4);

SamplerState Sampler : register(s0);

struct PSIn
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSIn IN) : SV_TARGET
{
    float4 diffuseCol = MaterialCB.UseDiffuseTexture ? Diffuse.Sample(Sampler, IN.TexCoord): MaterialCB.Diffuse;
        
    if(diffuseCol.a < 0.5)
        discard;
   
    float diffuse = saturate(dot(IN.Normal, LightCB.DirectionOrPosition));
    
    float halfVector = saturate(dot(IN.Normal, normalize(LightCB.DirectionOrPosition + float3(0, 0, 1))));
    float specular = pow(halfVector, 32);
    
    diffuse *= LightCB.Intensity;
    
    return float4((diffuse * LightCB.Color + float3(0.1, 0.1, 0.1) * specular + LightCB.AmbientColor) * diffuseCol.rgb, 1);
}