Texture2D Source : register(t0);
SamplerState Sampler : register(s0);

struct PSIn
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSIn IN) : SV_TARGET
{
    return Source.Sample(Sampler, IN.TexCoord);
}
