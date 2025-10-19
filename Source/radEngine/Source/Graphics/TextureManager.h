#pragma once

#include "Pipelines/GenerateMipsPipeline.h"
#include "RendererCommon.h"
#include "DXResource.h"
#include "EngineCommon.h"
#include "filesystem"
#include "Graphics/ResourcePool.h"

namespace rad
{
struct TextureIdType;

using TextureId = Id<TextureIdType>;

struct TextureManager
{
	TextureManager(rad::Renderer& renderer) : Renderer(renderer), GenerateMipsPipeline(renderer) {}
	bool Init();

	struct TextureLoadInfo
	{
		bool AlphaOnly = false;
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
	};

	void GenerateMips(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& texture);
	ResourcePool::OwnedResource* LoadTexture(std::filesystem::path const& path,
											 TextureLoadInfo const& info,
											 RenderGraphBuilder& rgBuilder,
											 bool generateMips = true);

private:
	std::unordered_map<TextureId, Ref<ResourcePool::OwnedResource>> Textures;
	std::unordered_map<std::filesystem::path, TextureId> LoadedTextures;
	Renderer& Renderer;
	TextureId NextId = {1};
	GenerateMipsPipeline GenerateMipsPipeline;
};

} // namespace rad
