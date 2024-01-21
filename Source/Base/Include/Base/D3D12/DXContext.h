#pragma once

#include "Base/D3D12/D3D12Common.h"
#include "Base/D3D12/Device.h"
#include "Base/D3D12/DXGIInterface.h"
namespace dfr::d3d12
{

struct DXContext
{
	DXContext() { Init(); }

	DXGIInterface* GetDXGIInterface() 
	{
		return Interface.get();
	}
	Device* GetDevice() 
	{
		return Dev.get();
	}

private:
	void Init();
	ru<Device> Dev{};
	ru<DXGIInterface> Interface{};
};

static ru<DXContext> GDXContext;
}