#pragma once

#include "Base/Common.h"

#include <wrl.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>

namespace dfr::d3d12
{
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct Device;
struct DXGIInterface;
struct CommandQueue;
struct CommandList;

struct DeviceChild
{
	DeviceChild(Device* dev) : Dev(dev) {}

	Device* Dev;
};

};