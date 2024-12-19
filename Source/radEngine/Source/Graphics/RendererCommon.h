#pragma once

#include "DXHelpers.h"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"

namespace rad
{
struct CommandContext
{
	RadDevice& Device;
	RadGraphicsCommandList& CommandList;
	std::unordered_map<D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorHeapPage*>& GPUHeapPages;
	std::vector<ComPtr<ID3D12Resource>>& IntermediateResources;
	operator RadGraphicsCommandList&()
	{
		return CommandList;
	}
	RadGraphicsCommandList* operator->()
	{
		return &CommandList;
	}
};
struct Renderer;
struct ShaderManager;
struct TextureManager;
struct ModelManager;
struct RenderFrameRecord;
} // namespace rad