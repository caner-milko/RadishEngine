#pragma once

#include "Pipelines/GenerateMipsPipeline.h"
#include "RendererCommon.h"
#include "DXResource.h"
#include "RadishCommon.h"
#include "filesystem"

RAD_ID_STRUCT_U32(rad, TextureId)

namespace rad
{

struct TextureManager
{
	TextureManager(rad::Renderer& renderer) : Renderer(renderer), GenerateMipsPipeline(renderer) {}
	bool Init();

	struct TextureLoadInfo
	{
		bool AlphaOnly = false;
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
	};

	void GenerateMips(CommandContext& commandCtx, DXTexture& texture);
	DXTexture* LoadTexture(std::filesystem::path const& path, TextureLoadInfo const& info, CommandContext& commandCtx, bool generateMips = true);

private:
	std::unordered_map<TextureId, std::unique_ptr<DXTexture>> Textures;
	std::unordered_map<std::filesystem::path, TextureId> LoadedTextures;
	Renderer& Renderer;
	TextureId NextId = { 1 };
	GenerateMipsPipeline GenerateMipsPipeline;
};

}

