#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

/*

H = maxDiff = max{b - b_i, i = 1,...,8}
a = cellArea
deltaS(basic) = H * a / 2 = maxDiff * cellArea / 2

*/

ConstantBuffer<ThermalDepositResources> Resources : register(b0);

// index = 0 => (-1, -1), 1 => (0, -1), 2 => (1, -1), 3=>(-1, 0), 4=>(1, 0), 5=>(-1, 1), 6=>(0, 1), 7=>(1, 1)
int2 IndexToOffset(uint index)
{
	int i = index + index / 4;
	return int2(i % 3 - 1, i / 3 - 1);
}
int OffsetToIndex(int2 offset)
{
	uint index = (offset.x + 1) + (offset.y + 1) * 3;
    return index - min(index / 4, 1);
}

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
		int2 offset = IndexToOffset(i);
		uint2 neighborCoord = dispatchID.xy + offset;
		if(IsInBounds(neighborCoord, textureSize))
		{
			uint index = OffsetToIndex(-offset);
			if(index > 3)
			{
				toAdd += pipes2[neighborCoord][index - 4];
			}
			else
			{
                toAdd += pipes1[neighborCoord][index];
            }
		}
		if(i > 3)
			toAdd -= pipes2[dispatchID.xy][i - 4];
		else
			toAdd -= pipes1[dispatchID.xy][i];
	}
	
    heightMap[dispatchID.xy] += toAdd;
}