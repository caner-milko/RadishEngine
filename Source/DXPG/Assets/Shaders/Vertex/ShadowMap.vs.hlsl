struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

StructuredBuffer<float3> Positions : register(t0);

struct VSIn
{
    uint PosIndex : POSINDEX;
    uint NormalIndex : NORMALINDEX;
    uint TexCoordIndex : TEXCOORDINDEX;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
};

VSOut main(VSIn IN)
{
    VSOut output;
    output.Pos = mul(ModelViewProjectionCB.MVP, float4(Positions[IN.PosIndex] / 100, 1.0));
    return output;
}