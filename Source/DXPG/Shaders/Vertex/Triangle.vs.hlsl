struct ModelViewProjection
{
    matrix MVP;
};
 
ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

StructuredBuffer<float3> Positions : register(t0);
StructuredBuffer<float3> Normals : register(t1);
StructuredBuffer<float3> TexCoords : register(t2);

struct VSIn
{
    uint PosIndex : POSINDEX;
    uint NormalIndex : NORMALINDEX;
    uint TexCoordIndex : TEXCOORDINDEX;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
    float3 Color : COLOR;
};

VSOut main(VSIn IN)
{
    VSOut output;
    output.Pos = mul(ModelViewProjectionCB.MVP, float4(Positions[IN.PosIndex], 1.0));
    output.Color = mul(ModelViewProjectionCB.MVP, float4(Normals[IN.NormalIndex], 0.0)).xyz;
    return output;
}