#include "TextureManager.h"

#include "Renderer.h"

#include <stb_image.h>

namespace rad
{

bool TextureManager::Init()
{
	return GenerateMipsPipeline.Setup();
}

void TextureManager::GenerateMips(CommandContext& commandCtx, DXTexture& texture)
{
	GenerateMipsPipeline.GenerateMips(commandCtx, texture);
}

DXTexture* rad::TextureManager::LoadTexture(std::filesystem::path const& path, TextureManager::TextureLoadInfo const& info, CommandContext& commandCtx, bool generateMips)
{
	auto it = LoadedTextures.find(path);
	if (it != LoadedTextures.end())
	{
		return Textures[it->second].get();
	}

	int width, height, comp;

	if (stbi_info(path.string().c_str(), &width, &height, &comp) == 0)
	{
		std::cout << "Failed to load texture info for " << path << ". Reason: " << stbi_failure_reason() << std::endl;
		return { 0 };
	}

	DXTexture::TextureCreateInfo createInfo = {
	.Width = uint32_t(width),
	.Height = uint32_t(height),
	.MipLevels = generateMips ? 0u : 1u,
	.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	.Flags = info.Flags,

	};

	if (generateMips)
		createInfo.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	int desiredComp = info.AlphaOnly ? STBI_grey : STBI_rgb_alpha;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &comp, desiredComp);

	if (data == nullptr)
	{
		std::cout << "Failed to load texture " << path << ". Reason: " << stbi_failure_reason() << std::endl;
		return { 0 };
	}

	auto texture = DXTexture::Create(Renderer.GetDevice(), path.filename().wstring(), createInfo, D3D12_RESOURCE_STATE_COPY_DEST);

	// Copy the data to the texture
	{
		size_t size = width * height * desiredComp;
		texture.UploadData(commandCtx, std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size_t(width * height * desiredComp)), desiredComp);
	}

	stbi_image_free(data);

	if (generateMips)
	{
		GenerateMips(commandCtx, texture);
	}


	TextureId id = NextId;
	NextId.Id++;
	
	auto& tex = Textures[id] = std::make_unique<DXTexture>(texture);
	LoadedTextures[path] = id;
	return tex.get();
}

}