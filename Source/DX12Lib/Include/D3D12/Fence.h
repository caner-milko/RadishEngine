#pragma once
#include "D3D12Common.h"
#include <d3d12.h>

namespace dfr::d3d12
{

struct Fence : DeviceChild
{
	struct FenceCreateInfo
	{
		uint64_t InitialValue = 0;
	};
	using DeviceChild::DeviceChild;

	void Init(FenceCreateInfo createInfo);
	void Wait(uint64_t waitValue);
	void Signal(uint64_t signalValue);
	uint64_t SignalNext()
	{
		Signal(++LastSignal);
		return LastSignal;
	}
	void WaitForLastSignal();
	bool Signalled();
	uint64_t Value();

	ComPtr<ID3D12Fence> DxFence;
	HANDLE OSEvent{};
	uint64_t LastSignal{};
};

}