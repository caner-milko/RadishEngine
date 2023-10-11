#pragma once
#include "D3D12Common.h"
#include "D3D12/Fence.h"

namespace dfr::d3d12
{

struct CommandList : public DeviceChild
{
	struct CommandListCreateInfo
	{
	};
	using DeviceChild::DeviceChild;

	bool Init(CommandListCreateInfo createInfo);

	void Begin();
	void Wait();

	ComPtr<ID3D12GraphicsCommandList2> DxCommandList;
	ru<Fence> CmdFence;
};

};