#pragma once
#include "D3D12Common.h"

namespace dfr::d3d12
{
struct CommandAllocator : public DeviceChild
{
	struct CommandAllocatorCreateInfo
	{
	};
	using DeviceChild::DeviceChild;

	void Init(CommandAllocatorCreateInfo createInfo);


	ComPtr<struct ID3D12CommandAllocator> DxCommandAllocator;
};
};