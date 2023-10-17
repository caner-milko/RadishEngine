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
	enum CommandListState
	{
		Recording,
		Closed,
		Executing,
		Executed
	};
	CommandList(CommandQueue* cmdQueue); 

	bool Init(CommandListCreateInfo createInfo);

	void Reset();
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
	CommandListState CmdState = CommandListState::Recording;
	std::vector<ComPtr<ID3D12Object>> Dependencies;
};

};