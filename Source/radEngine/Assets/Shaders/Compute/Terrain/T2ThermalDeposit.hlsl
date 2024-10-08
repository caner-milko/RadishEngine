#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

/*

H = maxDiff = max{b - b_i, i = 1,...,8}
a = cellArea
deltaS(basic) = H * a / 2 = maxDiff * cellArea / 2

*/

ConstantBuffer<ThermalDepositResources> Resources : register(b0);



bool IsInBounds(uint2 coord, uint2 textureSize)
{
	return coord.x < textureSize.x && coord.y < textureSize.y && coord.x >= 0 && coord.y >= 0;
}

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    RWTexture2D<float> heightMap = GetBindlessResource(Resources.OutHeightMapIndex);
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    Texture2D<float4> pipes1 = GetBindlessResource(Resources.InFluxTextureIndex1);
	Texture2D<float4> pipes2 = GetBindlessResource(Resources.InFluxTextureIndex2);

	float toAdd = 0;

	for(uint i = 0; i < 8; i++)
	{
		int2 offset = IndexToOffset8(i);
		uint2 neighborCoord = dispatchID.xy + offset;
		if(IsInBounds(neighborCoord, textureSize))
		{
			uint index = OffsetToIndex8(-offset);
			if(index > 3)
				toAdd += pipes2[neighborCoord][index - 4];
			else
                toAdd += pipes1[neighborCoord][index];
		}
		if(i > 3)
			toAdd -= pipes2[dispatchID.xy][i - 4];
		else
			toAdd -= pipes1[dispatchID.xy][i];
	}
	
    heightMap[dispatchID.xy] += toAdd;
}