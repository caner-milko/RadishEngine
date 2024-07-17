struct ModelViewProjection
{
    matrix MVP;
    matrix Normal;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VSIn
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent : TANGENT;
};

VSOut main(VSIn IN)
{
    VSOut output;
    output.Pos = mul(ModelViewProjectionCB.MVP, float4(IN.Pos, 1.0));
    output.Normal = normalize(mul((float3x3) ModelViewProjectionCB.Normal, IN.Normal));
    output.TexCoord = IN.TexCoord;
    output.Tangent = normalize(mul((float3x3) ModelViewProjectionCB.Normal, normalize(IN.Tangent)));
    return output;
}