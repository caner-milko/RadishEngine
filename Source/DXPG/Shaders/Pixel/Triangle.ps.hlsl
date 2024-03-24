struct PSIn
{
    float4 Position : SV_POSITION;
    float3 Color : COLOR;
};

float4 main(PSIn IN) : SV_TARGET
{
    return float4(IN.Color, 1.0);
}