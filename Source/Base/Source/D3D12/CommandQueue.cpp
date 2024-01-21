#include "Base/D3D12/CommandQueue.h"
#include "Base/D3D12/D3D12Common.h"
#include "Base/D3D12/Device.h"

namespace dfr::d3d12
{
CommandQueue::CommandQueue(Device* dev) : DeviceChild(dev), QueueFence(dev)
{
}
bool CommandQueue::Init(CommandQueueCreateInfo createInfo)
{
	ThrowIfFailed(Dev->DxDevice->CreateCommandQueue(&createInfo.Desc, IID_PPV_ARGS(&DxCommandQueue)));
	CommandLists.reserve(createInfo.CommandListCount);
	CommandList::CommandListCreateInfo cmdListCreateInfo = {.CommandListType = createInfo.Desc.Type };
	for (int i = 0; i < createInfo.CommandListCount; i++)
	{
		auto& cmdList = CommandLists.emplace_back(std::make_unique<CommandList>(this));
		cmdList->Init(cmdListCreateInfo);
	}
	QueueFence.Init({ .InitialValue = 0 });
	return true;
}
void CommandQueue::Flush()
{
	for (auto& cmdList : CommandLists)
	{
		cmdList->Wait();
	}
	for (auto& cmdList : CommandLists)
	{
		assert(cmdList->CmdFence.Signalled());
	}

	DxCommandQueue->Signal(QueueFence.DxFence.Get(), 1);

	QueueFence.Wait(1);

	QueueFence.DxFence->Signal(0);
}

CommandList* CommandQueue::BeginCommandList()
{
	auto start = NextCommandListIndex;
	CommandList* selected = nullptr;
	while (!selected)
	{
		auto& cmd = *CommandLists[NextCommandListIndex];
		if (cmd.CheckIfReady())
		{
			selected = &cmd;
			break;
		}
		NextCommandListIndex = (NextCommandListIndex+1)% CommandLists.size();
		if (NextCommandListIndex == start)
		{
			auto& startCmd = *CommandLists[NextCommandListIndex];
			if (startCmd.CheckIfReady())
			{
				selected = &cmd;
				break;
			}
		}
	}
	selected->Begin();
	return selected;
}
void CommandQueue::ExecuteCommandList(CommandList& cmdList)
{
	ID3D12CommandList* const ppCommandLists[] = {
		cmdList.DxCommandList.Get()
	};

	DxCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	
	DxCommandQueue->Signal(cmdList.CmdFence.DxFence.Get(), cmdList.CmdFence.SignalNext());

}
}