#pragma once
#include "D3D12Common.h"

namespace dfr::d3d12
{
struct CommandAllocator : public DeviceChild
{
	struct CommandAllocatorCreateInfo
	{
		D3D12_COMMAND_LIST_TYPE CommandListType;
	};
	using DeviceChild::DeviceChild;

	void Init(CommandAllocatorCreateInfo createInfo);
	void Reset();

	ComPtr<struct ID3D12CommandAllocator> DxCommandAllocator;
};
};