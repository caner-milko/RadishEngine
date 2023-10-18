#include "D3D12/Device.h"
#include "CppCommon.h"
#include "D3D12/CommandQueue.h"
#include "D3D12/CommandList.h"
namespace dfr
{
ru<d3d12::Device> GDxDev = nullptr;
}
namespace dfr::d3d12
{
CommandQueue* Device::GetImmediateCommandQueue()
{
	if (!ImmediateCommandQueue)
	{
		ImmediateCommandQueue = std::make_unique<CommandQueue>(this);
		ImmediateCommandQueue->Init({});
	}
	return ImmediateCommandQueue.get();
}

ComPtr<ID3D12DescriptorHeap> Device::CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(GDxDev->DxDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

UINT Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	return GDxDev->DxDevice->GetDescriptorHandleIncrementSize(type);
}

bool Device::InitDX(DeviceCreateInfo createInfo)
{
	assert(!GDxDev);
	GDxDev = ru<Device>( new Device() );
	return GDxDev->Init(createInfo);
}
bool Device::Init(DeviceCreateInfo createInfo)
{
	ThrowIfFailed(D3D12CreateDevice(createInfo.DXGIAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&DxDevice)));
	//    NAME_D3D12_OBJECT(d3d12Device2);

		// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(DxDevice.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif
	return true;
}
};