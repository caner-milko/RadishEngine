struct Material
{
    float4 Diffuse;
    int UseDiffuseTexture;
    int UseNormalMapTexture;
};
ConstantBuffer<Material> MaterialCB : register(b1);

Texture2D Diffuse : register(t0);
Texture2D NormalMap : register(t1);

SamplerState Sampler : register(s0);

struct PSIn
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent : TANGENT;
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
    if (MaterialCB.UseNormalMapTexture)
    {
        float3 normalMapVal = NormalMap.Sample(Sampler, IN.TexCoord).xyz * 2 - 1;
        normalMapVal.xy *= 3.0;
        normalMapVal = normalize(normalMapVal);
        
        float3 normal = normalize(IN.Normal);
        float3 tangent = normalize(IN.Tangent);
        float3 bitangent = normalize(cross(normal, tangent));
        
        //Calculate TBN matrix
        float3x3 TBN = float3x3(tangent, bitangent, normal);
        output.Normal = float4(normalize(mul(normalMapVal, TBN)), 0);
    }
    else
        output.Normal = float4(IN.Normal, 0);
    return output;
}