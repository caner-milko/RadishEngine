struct VSOut
{
    float4 position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOut main(uint vI : SV_VERTEXID)
{
    float2 texcoord = float2(vI & 1, vI >> 1); //you can use these for texture coordinates later
    VSOut output;
    output.position = float4(texcoord.x * 2 - 1, 1 - texcoord.y * 2, 0, 1);
    output.TexCoord = texcoord;
    return output;
}