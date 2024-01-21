#include "Base/D3D12/CommandAllocator.h"
#include "Base/D3D12/D3D12Common.h"
#include "Base/D3D12/Device.h"
namespace dfr::d3d12
{
void CommandAllocator::Init(CommandAllocatorCreateInfo createInfo)
{
	ThrowIfFailed(Dev->DxDevice->CreateCommandAllocator(createInfo.CommandListType, IID_PPV_ARGS(&DxCommandAllocator)));
}
void CommandAllocator::Reset()
{
	ThrowIfFailed(DxCommandAllocator->Reset());
}
}