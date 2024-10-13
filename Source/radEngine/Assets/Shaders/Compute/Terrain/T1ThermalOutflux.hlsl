#include "TerrainCommon.hlsli"

ConstantBuffer<ThermalOutfluxResources> Resources : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.InHeightMapIndex);
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    RWTexture2D<float4> outPipes1 = GetBindlessResource(Resources.OutFluxTextureIndex1);
	RWTexture2D<float4> outPipes2 = GetBindlessResource(Resources.OutFluxTextureIndex2);
	float texelSize = 1 / float(textureSize.x);

	float heightCur = heightMap[dispatchID.xy];
	float heightDiffs[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
	for(uint i = 0; i < 8; i++)
	{
		int2 offset = IndexToOffset8(i);
		uint2 neighborCoord = dispatchID.xy + offset;
		if(IsInBounds(neighborCoord, textureSize))
		{
			float heightNeighbor = heightMap[neighborCoord];
			float heightDif = heightCur - heightNeighbor;
			heightDiffs[i] = heightDif;
		}
	}

	float totDiff = 0;
	for(uint i = 0; i < 8; i++)
        totDiff += max(heightDiffs[i], 0);

    float deltaS = totDiff * Resources.ThermalErosionRate * (Resources.PipeLength * Resources.PipeLength) / 2 * Resources.DeltaTime;
	
	float effectiveTotalHeightDiffs = 0;
	
	for(uint i = 0; i < 8; i++)
	{
		int2 offset = IndexToOffset8(i);
        float d = length(float2(offset)) * texelSize;
        float heightDiff = heightDiffs[i];
		if(heightDiff > 0)
		{
            if (heightDiff / d >= Resources.TalusAnglePrecomputed)
			{
				effectiveTotalHeightDiffs += heightDiff;
				continue;
			}
		}
		heightDiffs[i] = -1;
	}

	float4 outFlux1 = 0;
	float4 outFlux2 = 0;

	for(uint i = 0; i < 8; i++)
	{
		int2 offset = IndexToOffset8(i);
		float heightDiff = heightDiffs[i];
		if(heightDiff > 0)
		{
			float outFlux = deltaS * heightDiff / effectiveTotalHeightDiffs;
			uint2 neighborCoord = dispatchID.xy + offset;
			if(i > 3)
				outFlux2[i - 4] = outFlux;
			else
				outFlux1[i] = outFlux;
		}
	}

	outPipes1[dispatchID.xy] = outFlux1;
    outPipes2[dispatchID.xy] = outFlux2;
}