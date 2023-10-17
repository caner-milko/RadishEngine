#pragma once

#include "D3D12/D3D12Common.h"
#include "D3D12/CommandQueue.h"


namespace dfr::d3d12
{
struct Device
{
	struct DeviceCreateInfo
	{
		ComPtr<IDXGIAdapter4> DXGIAdapter;
	};
	static bool InitDX(DeviceCreateInfo createInfo);

	CommandQueue* GetImmediateCommandQueue();
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
	UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

	ComPtr<ID3D12Device2> DxDevice;
	std::unique_ptr<CommandQueue> ImmediateCommandQueue;
private:
	Device() {}

	bool Init(DeviceCreateInfo createInfo);
};
}
namespace dfr
{
extern ru<d3d12::Device> GDxDev;
}