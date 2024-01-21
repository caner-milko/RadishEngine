#include "Base/D3D12/DXContext.h"
#include "Base/D3D12/Device.h"
namespace dfr::d3d12
{
void DXContext::Init()
{
	Interface = std::make_unique<DXGIInterface>();
	Device::DeviceCreateInfo devInfo = {.Interface = GetDXGIInterface()};
	Dev = std::make_unique<d3d12::Device>();
	Dev->Init(devInfo);
}
}