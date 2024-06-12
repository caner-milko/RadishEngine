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

struct PSOut
{
    float4 Albedo : SV_TARGET;
    float4 Normal : SV_TARGET1;
};

PSOut main(PSIn IN)
{
    float4 diffuseCol = MaterialCB.UseDiffuseTexture ? Diffuse.Sample(Sampler, IN.TexCoord): MaterialCB.Diffuse;
        
    if(diffuseCol.a < 0.5)
        discard;
   
    PSOut output;
    output.Albedo = diffuseCol;
    output.Normal = float4(IN.Normal, 1);
    return output;
}