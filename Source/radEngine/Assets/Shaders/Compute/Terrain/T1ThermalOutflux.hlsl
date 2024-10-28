#include "TerrainCommon.hlsli"

ConstantBuffer<ThermalOutfluxResources> Resources : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float> heightMap = GetBindlessResource(Resources.InHeightMapIndex);
    Texture2D<float> hardnessMap = GetBindlessResource(Resources.InHardnessMapIndex);
    uint2 textureSize;
    heightMap.GetDimensions(textureSize.x, textureSize.y);
    RWTexture2D<float4> outPipes1 = GetBindlessResource(Resources.OutFluxTextureIndex1);
	RWTexture2D<float4> outPipes2 = GetBindlessResource(Resources.OutFluxTextureIndex2);

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

	float hardness = hardnessMap[dispatchID.xy];
	
    float effectiveTotalHeightDiffs = 0;
    float maxHeightDiff = 0;
	
    float talusAngle = hardness * (1.0 - Resources.SoftnessTalusCoefficient) + Resources.MinTalusCoefficient;
	
    for (uint i = 0; i < 8; i++)
    {
        int2 offset = IndexToOffset8(i);
        float d = length(float2(offset)) * Resources.PipeLength;
        float heightDiff = heightDiffs[i];
        if (heightDiff > 0)
        {
            if ((heightDiff / d) > talusAngle)
            {
                effectiveTotalHeightDiffs += heightDiff;
				maxHeightDiff = max(maxHeightDiff, heightDiff);
                continue;
            }
        }
        heightDiffs[i] = -1;
    }
    

	float cellArea = Resources.PipeLength * Resources.PipeLength;
	
    float deltaS = cellArea * effectiveTotalHeightDiffs * Resources.DeltaTime * hardness * Resources.ThermalErosionRate * 0.5;
	
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