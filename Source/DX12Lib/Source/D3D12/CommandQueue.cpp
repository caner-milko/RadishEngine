#include "D3D12/CommandQueue.h"
#include "D3D12/D3D12Common.h"
#include "D3D12/Device.h"

namespace dfr::d3d12
{
bool CommandQueue::Init(CommandQueueCreateInfo createInfo)
{
    ThrowIfFailed(Dev->DXDevice->CreateCommandQueue(&createInfo.Desc, IID_PPV_ARGS(&DxCommandQueue)));
    return true;
}
void CommandQueue::Flush()
{
    for (auto& cmdList : CommandLists)
    {
        cmdList->Wait();
    }
}

CommandList* CommandQueue::BeginCommandList()
{
    return nullptr;
}
}