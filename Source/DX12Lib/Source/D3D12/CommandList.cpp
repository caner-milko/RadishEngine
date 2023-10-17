#include "D3D12/CommandList.h"
#include "D3D12/D3D12Common.h"
#include "D3D12/CommandQueue.h"
#include "D3D12/Device.h"

namespace dfr::d3d12
{
CommandList::CommandList(CommandQueue* cmdQueue) : CmdQueue(cmdQueue), DeviceChild(cmdQueue->Dev), CmdAllocator(cmdQueue->Dev), CmdFence(cmdQueue->Dev)
{
}
bool CommandList::Init(CommandListCreateInfo createInfo)
{
	CmdAllocator.Init({ .CommandListType = createInfo.CommandListType });
	ThrowIfFailed(GDxDev->DxDevice->CreateCommandList(0, createInfo.CommandListType, CmdAllocator.DxCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&DxCommandList)));
	CmdFence.Init({});
	return true;
}
void CommandList::Reset()
{
    assert(CmdFence.Signalled());
    if (CmdState == CommandListState::Recording)
    {
        Close();
        Clear();
    }
    ThrowIfFailed(DxCommandList->Reset(CmdAllocator.DxCommandAllocator.Get(), nullptr));
    CmdState = CommandListState::Recording;
}
void CommandList::Close()
{
    assert(CmdState == CommandListState::Recording);
    ThrowIfFailed(DxCommandList->Close());
    CmdState = CommandListState::Closed;
}
CommandList& CommandList::Execute()
{
    if(CmdState == CommandListState::Recording)
        Close();
    
    assert(CmdState == CommandListState::Closed);

    CmdQueue->ExecuteCommandList(*this);

    CmdFence.SignalNext();
    CmdAllocator.Release();
    CmdState = CommandListState::Executing;
    return *this;
}
void CommandList::Wait()
{
    if (CmdState == CommandListState::Executing)
    {
        CmdFence.WaitForLastSignal();
        Clear();
    }
}

void CommandList::Clear()
{
    assert(CmdFence.Signalled());
    CmdAllocator.Reset();
    Dependencies.clear();
    CmdState = CommandListState::Executed;
}
bool CommandList::CheckIfReady()
{
    if (CmdState != CommandListState::Executing)
        return true;
    if (CmdFence.Signalled())
    {
        Clear();
        return true;
    }
    return false;
}
void CommandList::AddDependency(ComPtr<ID3D12Object> resource)
{
    assert(CmdState == CommandListState::Recording);
    Dependencies.push_back(resource);
}
}