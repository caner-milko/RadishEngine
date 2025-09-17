#include "TextureManager.h"

#include "Renderer.h"
#include "RenderGraphHelpers.h"
#include <stb_image.h>

namespace rad
{

bool TextureManager::Init()
{
	return GenerateMipsPipeline.Setup();
}

void TextureManager::GenerateMips(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& texture)
{
	GenerateMipsPipeline.GenerateMips(rgBuilder, texture);
}

ResourcePool::OwnedResource* rad::TextureManager::LoadTexture(std::filesystem::path const& path,
															  TextureManager::TextureLoadInfo const& info,
															  RenderGraphBuilder& rgBuilder,
															  bool generateMips)
{
	auto it = LoadedTextures.find(path);
	if (it != LoadedTextures.end())
	{
		return &Textures[it->second];
	}

	int width, height, comp;

	if (stbi_info(path.string().c_str(), &width, &height, &comp) == 0)
	{
		std::cout << "Failed to load texture info for " << path << ". Reason: " << stbi_failure_reason() << std::endl;
		return nullptr;
	}

	int desiredComp = info.AlphaOnly ? STBI_grey : STBI_rgb_alpha;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &comp, desiredComp);
	if (data == nullptr)
	{
		std::cout << "Failed to load texture " << path << ". Reason: " << stbi_failure_reason() << std::endl;
		return {0};
	}

	auto& res = Renderer.ResourcePool->GetResource(
		ResourceCreateHelper::Texture2D(width,
										height,
										DXGI_FORMAT_R8G8B8A8_UNORM,
										generateMips ? ResourcePresetFlags::MipMaps : ResourcePresetFlags::None,
										ResourceCreateHelper::TextureDetails{.DetailedFlags = info.Flags}),
		path.filename().generic_string());

	auto rgRes = rgBuilder.GetOrAddExternalResource(res.AsView());

	// Copy the data to the texture
	{
		size_t size = width * height * desiredComp;
		std::vector<std::byte> imgData = std::vector<std::byte>(reinterpret_cast<std::byte const*>(data),
																reinterpret_cast<std::byte const*>(data) + size);
		rghelpers::UploadTextureData(rgBuilder, rgRes, imgData, desiredComp);
	}

	stbi_image_free(data);

	if (generateMips)
	{
		GenerateMips(rgBuilder, rgRes);
	}

	TextureId id = NextId;
	NextId.Value++;

	Textures.insert_or_assign(id, res);
	LoadedTextures[path] = id;
	return &res;
}

} // namespace rad