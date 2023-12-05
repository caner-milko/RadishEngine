#pragma once
#include "D3D12Common.h"
#include "D3D12/Fence.h"
#include "D3D12/CommandAllocator.h"

namespace dfr::d3d12
{

struct CommandList : public DeviceChild
{
	struct CommandListCreateInfo
	{
		D3D12_COMMAND_LIST_TYPE CommandListType;
	};	
	enum CommandListState : uint32_t
	{
		Initial=0b1,
		Recording=0b10,
		Closed=0b100,
		Executing = 0b1000
	};
	CommandList(CommandQueue* cmdQueue); 
	~CommandList()
	{
		Wait();
	}
	bool Init(CommandListCreateInfo createInfo);

	void Begin();
	void Close();
	CommandList& Execute();
	void Wait();
	void Clear();
	bool CheckIfReady();
	void AddDependency(ComPtr<ID3D12Object> resource);

	CommandQueue* CmdQueue = nullptr;
	ComPtr<ID3D12GraphicsCommandList2> DxCommandList;
	CommandAllocator CmdAllocator;
	Fence CmdFence;
	CommandListState CmdState = CommandListState::Initial;
	std::vector<ComPtr<ID3D12Object>> Dependencies;
};

};