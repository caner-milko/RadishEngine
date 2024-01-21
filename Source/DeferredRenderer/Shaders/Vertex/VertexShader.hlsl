struct ModelViewProjection
{
	matrix MVP;
};

struct VertexIndices
{
	uint PositionIndex;
	uint NormalIndex;
	uint TexCoordsIndex;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

StructuredBuffer<VertexIndices> Indices : register(t0);
StructuredBuffer<float3> Positions : register(t1);
StructuredBuffer<float2> TexCoords : register(t2);
StructuredBuffer<float3> Normals : register(t3);

struct VertexShaderOutput
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD;
	float3 Normal   : NORMAL;
};

VertexShaderOutput main(uint vertexID : SV_VertexID)
{
	VertexShaderOutput OUT;

	VertexIndices indices = Indices[vertexID];

	OUT.Position = mul(ModelViewProjectionCB.MVP, float4(Positions[indices.PositionIndex], 1.0f));
	OUT.Normal = Normals[indices.NormalIndex];
	OUT.TexCoord = TexCoords[indices.TexCoordsIndex];

	return OUT;
}