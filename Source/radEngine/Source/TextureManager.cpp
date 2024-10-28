#include "TextureManager.h"

#include <stb_image.h>

namespace rad
{

std::unique_ptr<TextureManager> TextureManager::Instance = nullptr;
void TextureManager::Init(ID3D12Device2* device)
{
	Device = device;
	GenerateMipsPipeline.Setup(device);
}

void TextureManager::GenerateMips(FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, DXTexture& texture)
{
	GenerateMipsPipeline.GenerateMips(frameCtx, cmdList, texture);
}

DXTexture* rad::TextureManager::LoadTexture(std::filesystem::path const& path, TextureManager::TextureLoadInfo const& info, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, bool generateMips)
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

	auto texture = DXTexture::Create(Device, path.filename().wstring(), createInfo, D3D12_RESOURCE_STATE_COPY_DEST);

	// Copy the data to the texture
	{
		size_t size = width * height * desiredComp;
		texture.UploadData(frameCtx, cmdList, std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size_t(width * height * desiredComp)), desiredComp);
	}

	stbi_image_free(data);

	if (generateMips)
	{
		GenerateMips(frameCtx, cmdList, texture);
	}


	TextureId id = NextId;
	NextId.Id++;
	
	auto& tex = Textures[id] = std::make_unique<DXTexture>(texture);
	LoadedTextures[path] = id;
	return tex.get();
}

}