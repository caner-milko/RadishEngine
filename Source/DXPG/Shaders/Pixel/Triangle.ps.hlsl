struct Material
{
    float4 Diffuse;
    int UseDiffuseTexture;
    int UseAlphaMask;
};
ConstantBuffer<Material> MaterialCB : register(b1);

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
    float4 diffuse = MaterialCB.UseDiffuseTexture ? Diffuse.Sample(Sampler, IN.TexCoord): MaterialCB.Diffuse;
        
    if(diffuse.a < 0.5)
        discard;
   
    return diffuse;
}