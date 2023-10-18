#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shlwapi.h>

#include <Application.h>
#include <DeferredRenderer.h>

#include <dxgidebug.h>

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	int retCode = 0;

	// Set the working directory to the path of the executable.
	WCHAR path[MAX_PATH];
	HMODULE hModule = GetModuleHandleW(NULL);
	if ( GetModuleFileNameW(hModule, path, MAX_PATH) > 0 )
	{
		PathRemoveFileSpecW(path);
		SetCurrentDirectoryW(path);
	}

	dfr::Application::Create(hInstance);
	{
		std::shared_ptr<dfr::DeferredRenderer> demo = std::make_shared<dfr::DeferredRenderer>(L"DX12 Deferred Renderer", 1280, 720);
		retCode = dfr::Application::Get().Run(demo);
	}
	dfr::Application::Destroy();

	atexit(&ReportLiveObjects);

	return retCode;
}