#include "Base/D3D12/Fence.h"
#include "Base/D3D12/Device.h"
namespace dfr::d3d12
{

void Fence::Init(FenceCreateInfo createInfo)
{
	ThrowIfFailed(GDxDev->DxDevice->CreateFence(createInfo.InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&DxFence)));
	OSEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(OSEvent && "Failed to create fence event handle.");
}
void Fence::Wait(uint64_t waitValue)
{
	if (Value() >= waitValue)
		return;
	DxFence->SetEventOnCompletion(waitValue, OSEvent);
	::WaitForSingleObject(OSEvent, DWORD_MAX);
}

void Fence::Signal(uint64_t signalValue)
{
	LastSignal = signalValue;
}

void Fence::WaitForLastSignal()
{
	Wait(LastSignal);
}

bool Fence::Signalled()
{
	return Value() == LastSignal;
}

uint64_t Fence::Value()
{
	return DxFence->GetCompletedValue();
}
}