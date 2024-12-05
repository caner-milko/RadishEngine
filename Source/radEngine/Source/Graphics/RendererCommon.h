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
	operator RadGraphicsCommandList& () { return CommandList; }
	RadGraphicsCommandList* operator->() { return &CommandList; }
};
struct Renderer;
struct ShaderManager;
struct TextureManager;
struct ModelManager;
struct RenderFrameRecord;

struct Renderable
{
    std::string Name;
    uint32_t MaterialBufferIndex;
	uint32_t DiffuseTextureIndex;
	uint32_t NormalMapTextureIndex;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
    Matrix4x4 GlobalModelMatrix;

	__forceinline uint32_t GetIndexCount() const
	{
		return IndexBufferView.SizeInBytes / sizeof(uint32_t);
	}
};
}