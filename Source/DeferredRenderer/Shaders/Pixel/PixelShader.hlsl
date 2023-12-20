struct PixelShaderInput
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
};

float4 main( PixelShaderInput IN ) : SV_Target
{
    return float4(IN.TexCoord, 0, 0);
}