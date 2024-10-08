#pragma once
#include "BindlessRootSignature.hlsli"
#include "TerrainConstantBuffers.hlsli"
#include "TerrainResources.hlsli"

bool IsInBounds(uint2 coord, uint2 textureSize)
{
    return coord.x < textureSize.x && coord.y < textureSize.y && coord.x >= 0 && coord.y >= 0;
}

// index = 0 => (-1, -1), 1 => (0, -1), 2 => (1, -1), 3=>(-1, 0), 4=>(1, 0), 5=>(-1, 1), 6=>(0, 1), 7=>(1, 1)
int2 IndexToOffset8(uint index)
{
    int i = index + index / 4;
    return int2(i % 3 - 1, i / 3 - 1);
}
int OffsetToIndex8(int2 offset)
{
    uint index = (offset.x + 1) + (offset.y + 1) * 3;
    return index - min(index / 4, 1);
}

// 0=>(-1, 0), 1=>(0, -1), 2=>(1, 0), 3=>(0, 1)
int2 IndexToOffset4(int index)
{
    return int2(((index + 1) % 2) * ((index / 2) * 2 - 1), (index % 2) * ((index / 2) * 2 - 1));
}

int OffsetToIndex4(int2 offset)
{
    return abs(offset.x) * (offset.x + 1) + abs(offset.y) * (offset.y + 2);
}