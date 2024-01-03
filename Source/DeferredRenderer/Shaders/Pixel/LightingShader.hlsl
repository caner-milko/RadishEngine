struct PixelShaderInput
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
};

struct PS_OUTPUT
{
    float4 Albedo  : SV_Target0;
    float4 Normal : SV_Target1;
};

PS_OUTPUT main( PixelShaderInput psIn ) 
{
    PS_OUTPUT psout;
    psout.Albedo = float4(psIn.TexCoord, 0, 1);
    psout.Normal = float4(psIn.Normal, 1);
    
    return psout;
}