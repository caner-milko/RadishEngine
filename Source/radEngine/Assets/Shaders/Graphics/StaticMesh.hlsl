#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

SamplerState Sampler : register(s2);

ConstantBuffer<StaticMeshResources> Resources : register(b0);

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

[RootSignature(BindlessRootSignature)]
VSOut VSMain(VSIn IN)
{
    VSOut output;
    output.Pos = mul(Resources.MVP, float4(IN.Pos, 1.0));
    output.Normal = normalize(IN.Normal);
    output.TexCoord = IN.TexCoord;
    output.Tangent = normalize(IN.Tangent);
    return output;
}

struct PSOut
{
    float4 Albedo : SV_TARGET;
    float4 Normal : SV_TARGET1;
};

[RootSignature(BindlessRootSignature)]
PSOut PSMain(VSOut IN)
{
	ConstantBuffer<MaterialBuffer> material = GetBindlessResource(Resources.MaterialBufferIndex);
    float4 diffuseCol = material.Diffuse;
	if(material.DiffuseTextureIndex)
	{
        Texture2D<float4> diffuseTex = GetBindlessResource(material.DiffuseTextureIndex);
        diffuseCol = diffuseTex.Sample(Sampler, IN.TexCoord);
	}
    
    if(diffuseCol.a < 0.5)
        discard;
   
    PSOut output;
    output.Albedo = diffuseCol;
    if (material.NormalMapTextureIndex)
    {
		Texture2D<float4> normalMap = GetBindlessResource(material.NormalMapTextureIndex);
        float3 normalMapVal = normalMap.Sample(Sampler, IN.TexCoord).xyz * 2 - 1;
        //normalMapVal.xy *= 3.0;
        normalMapVal = normalize(normalMapVal);
        normalMapVal.x *= -1;
        
        float3 normal = normalize(IN.Normal);
        float3 tangent = normalize(IN.Tangent);
        float3 bitangent = normalize(cross(normal, tangent));
        
        //Calculate TBN matrix
        float3x3 TBN = float3x3(tangent, bitangent, normal);
        output.Normal = float4(normalize(mul(normalMapVal, TBN)), 0);
        //output.Normal = float4(normalize(mul((float3x3) Resources.Normal, output.Normal.xyz)), 0);
        //output.Albedo = float4(normalMapVal, 1);
    }
    else
        output.Normal = float4(IN.Normal, 0);
    return output;
}