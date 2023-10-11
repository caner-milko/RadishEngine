#pragma once
#include "D3D12Common.h"

namespace dfr::d3d12
{

struct Fence : DeviceChild
{
	struct FenceCreateInfo
	{
	};
	using DeviceChild::DeviceChild;

	void Init(FenceCreateInfo createInfo);

	ComPtr<struct ID3D12Fence> DxFence;
};

}