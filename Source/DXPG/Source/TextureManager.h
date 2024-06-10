#pragma once

#include "DXResource.h"
#include "DXPGCommon.h"
#include "filesystem"
#include "Pipelines/GenerateMipsPipeline.h"

DXPG_ID_STRUCT_U32(dxpg, TextureId)

namespace dxpg
{

struct TextureManager : public Singleton<TextureManager>
{
	void Init(ID3D12Device2* device);

	struct TextureLoadInfo
	{
		bool AlphaOnly = false;
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
	};

	DXTexture* LoadTexture(std::filesystem::path const& path, TextureLoadInfo const& info, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, bool generateMips = true);

private:
	std::unordered_map<TextureId, std::unique_ptr<DXTexture>> Textures;
	std::unordered_map<std::filesystem::path, TextureId> LoadedTextures;
	ID3D12Device2* Device;
	TextureId NextId = { 1 };
	GenerateMipsPipeline GenerateMips;
};

}

