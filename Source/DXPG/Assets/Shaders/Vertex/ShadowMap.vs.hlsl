struct ModelViewProjection
{
    matrix MVP;
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
};

VSOut main(VSIn IN)
{
    VSOut output;
    output.Pos = mul(ModelViewProjectionCB.MVP, float4(IN.Pos, 1.0));
    return output;
}