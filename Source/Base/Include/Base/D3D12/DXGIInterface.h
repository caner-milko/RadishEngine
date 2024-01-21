#pragma once
#include <glm/vec2.hpp>

#include "Base/D3D12/D3D12Common.h"

namespace dfr::d3d12
{
struct DXGIInterface 
{
	DXGIInterface() = default;
	void Init();

	ComPtr<IDXGISwapChain4> CreateSwapchain(HWND hWnd, glm::uvec2 extent, uint32_t bufferCount, CommandQueue* cmdQueue, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	ComPtr<IDXGIFactory4> DXGIFactory{};
	ComPtr<IDXGIAdapter4> DXGIAdapter{};
	bool IsTearingSupported{ false };
};
}