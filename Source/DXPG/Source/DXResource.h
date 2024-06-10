#pragma once

#include "DXHelpers.h"

namespace dxpg
{


struct DXResource
{
	ComPtr<ID3D12Resource> Resource = nullptr;
	D3D12_RESOURCE_STATES State;

	// Remove this with multi threaded rendering
	D3D12_RESOURCE_BARRIER Transition(D3D12_RESOURCE_STATES newState)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(), State, newState);
		State = newState;
		return barrier;
	}

protected:
	DXResource(std::wstring name, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES startState) noexcept : Resource(resource), State(startState) {}
};

struct DXTexture : DXResource
{
	struct TextureCreateInfo
	{
		D3D12_RESOURCE_DIMENSION Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		uint32_t Width;
		uint32_t Height;
		//Multipled by 6 for cube maps
		uint16_t DepthOrArraySize = 1;
		uint16_t MipLevels = 0;
		DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_SAMPLE_DESC SampleDesc = { 1, 0 };
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_TEXTURE_LAYOUT Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		uint64_t Alignment = 0;
		std::optional<D3D12_CLEAR_VALUE> ClearValue = std::nullopt;
		bool IsCubeMap = false;

		TextureCreateInfo& AddFlags(D3D12_RESOURCE_FLAGS flags)
		{
			Flags |= flags;
			return *this;
		}
	};

	static DXTexture Create(ID3D12Device* device, std::wstring name, TextureCreateInfo const& info, D3D12_RESOURCE_STATES startState = D3D12_RESOURCE_STATE_COMMON);

	std::unique_ptr<ShaderResourceView> CreateSRV(D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc);
	std::unique_ptr<UnorderedAccessView> CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc);
	std::unique_ptr<RenderTargetView> CreateRTV(D3D12_RENDER_TARGET_VIEW_DESC const* rtvDesc);
	std::unique_ptr<DepthStencilView> CreateDSV(D3D12_DEPTH_STENCIL_VIEW_DESC const* dsvDesc);

	void CreatePlacedSRV(DescriptorAllocationView alloc, D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc);
	void CreatePlacedUAV(DescriptorAllocationView alloc, D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc);
	void CreatePlacedRTV(DescriptorAllocationView alloc, D3D12_RENDER_TARGET_VIEW_DESC const* rtvDesc);
	void CreatePlacedDSV(DescriptorAllocationView alloc, D3D12_DEPTH_STENCIL_VIEW_DESC const* dsvDesc);

	TextureCreateInfo Info;

protected:
	DXTexture(std::wstring name, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES startState, TextureCreateInfo const& info) noexcept : DXResource(name, resource, startState), Info(info) {}
};

struct DXBuffer : DXResource
{
	static DXBuffer Create(ID3D12Device* device, std::wstring name, size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	std::unique_ptr<ShaderResourceView> CreateSRV(size_t size = 0, size_t offset = 0, size_t stride = 0, D3D12_BUFFER_SRV_FLAGS flags = D3D12_BUFFER_SRV_FLAG_NONE);

	template<typename T>
	std::unique_ptr<ShaderResourceView> CreateTypedSRV(size_t size = 0, size_t offset = 0, D3D12_BUFFER_SRV_FLAGS flags = D3D12_BUFFER_SRV_FLAG_NONE)
	{
		return CreateSRV(size, offset, sizeof(T), flags);
	}

	std::unique_ptr<UnorderedAccessView> CreateUAV(size_t numElements, size_t stride = 0, size_t firstElement = 0, D3D12_BUFFER_UAV_FLAGS flags = D3D12_BUFFER_UAV_FLAG_NONE);

	template<typename T>
	std::unique_ptr<UnorderedAccessView> CreateTypedUAV(size_t firstElement = 0, D3D12_BUFFER_UAV_FLAGS flags = D3D12_BUFFER_UAV_FLAG_NONE)
	{
		return CreateUAV(Size/sizeof(T), sizeof(T), firstElement, flags);
	}

	std::unique_ptr<ConstantBufferView> CreateCBV(size_t size = 0, size_t offset = 0);
	template<typename T>
	std::unique_ptr<ConstantBufferView> CreateTypedCBV(size_t size = 0, size_t offset = 0)
	{
		return CreateCBV(size, offset);
	}

	inline D3D12_GPU_VIRTUAL_ADDRESS GPUAddress(size_t offset = 0)
	{
		return Resource->GetGPUVirtualAddress() + offset;
	}

	template<typename T = void>
	inline T* Map()
	{
		assert(sizeof(T) <= Size);
		T* Data = nullptr;
		Resource->Map(0, nullptr, (void**)&Data);
		return Data;
	}

	size_t Size = 0;

protected:
	DXBuffer(std::wstring name, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES startState, size_t size) noexcept : DXResource(name, resource, startState), Size(size) {}
};

}