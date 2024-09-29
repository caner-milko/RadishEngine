#pragma once

#include "RadishCommon.h"
#include "DXResource.h"
#include "RendererCommon.h"
#include "Model.h"

namespace rad::proc
{

struct TerrainData
{
	uint32_t MeshResX = 256, MeshResY = 256;
	StandaloneModel Model{};
	DXTexture HeightMap{};
};

struct TerrainGenerator
{
	std::vector<float> CreateDiamondSquareHeightMap(uint32_t toPowerOfTwo, uint32_t& width);
	TerrainData InitializeTerrain(ID3D12Device* device, uint32_t resX, uint32_t resY);
	void GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList* cmdList, TerrainData& terrain, uint32_t toPowerOfTwo, bool createMesh = true);
};

}