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
	CmdState = CommandListState::Initial;
	return true;
}

void CommandList::Begin()
{
	assert(CmdState & CommandListState::Initial);
	CmdState = CommandListState::Recording;
}

void CommandList::Close()
{
	assert(CmdState & CommandListState::Recording);
	ThrowIfFailed(DxCommandList->Close());
	CmdState = CommandListState::Closed;
}

CommandList& CommandList::Execute()
{
	if(CmdState & CommandListState::Recording)
		Close();
	
	assert(CmdState & CommandListState::Closed);

	CmdQueue->ExecuteCommandList(*this);
	CmdState = CommandListState::Executing;
	return *this;
}

void CommandList::Wait()
{
	assert(CmdState & (CommandListState::Initial | CommandListState::Executing));

	CmdFence.WaitForLastSignal();
	Clear();
}

void CommandList::Clear()
{
	assert(CmdState & (CommandListState::Initial | CommandListState::Executing));
	if (CmdState & CommandListState::Initial)
	{
		assert(Dependencies.empty());
		return;
	}
	assert(CmdFence.Signalled());
	if (CmdState != CommandListState::Executing)
		Close();
	//if (CmdState != CommandListState::Initial)
	{
		CmdAllocator.Reset();
		ThrowIfFailed(DxCommandList->Reset(CmdAllocator.DxCommandAllocator.Get(), nullptr));
	}
	Dependencies.clear();
	CmdState = CommandListState::Initial;
}

bool CommandList::CheckIfReady()
{
	if (CmdState != CommandListState::Executing)
	{
		assert(CmdFence.Signalled());
		if (CmdState & CommandListState::Initial)
			return true;
		Clear();
		return true;
	}
	if (CmdFence.Signalled())
	{
		Clear();
		return true;
	}
	return false;
}

}