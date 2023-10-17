#include <Application.h>
#include <Window.h>
#include <Game.h>
#include "D3D12/Device.h"
#include "D3D12/CommandQueue.h"

namespace dfr
{

Window::Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync )
    : hWnd(hWnd)
    , WindowName(windowName)
    , ClientWidth(clientWidth)
    , ClientHeight(clientHeight)
    , VSync(vSync)
    , Fullscreen(false)
    , FrameCounter(0)
{
    Application& app = Application::Get();

    IsTearingSupported = app.IsTearingSupported();

    DXGISwapChain = CreateSwapChain();
    D3D12RTVDescriptorHeap = GDxDev->CreateDescriptorHeap(BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    RTVDescriptorSize = GDxDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews();
}

Window::~Window()
{
    // Window should be destroyed with Application::DestroyWindow before
    // the window goes out of scope.
    assert(!hWnd && "Use Application::DestroyWindow before destruction.");
}

HWND Window::GetWindowHandle() const
{
    return hWnd;
}

const std::wstring& Window::GetWindowName() const
{
    return WindowName;
}

void Window::Show()
{
    ::ShowWindow(hWnd, SW_SHOW);
}

/**
* Hide the window.
*/
void Window::Hide()
{
    ::ShowWindow(hWnd, SW_HIDE);
}

void Window::Destroy()
{
    if (auto pGame = GamePtr.lock())
    {
        // Notify the registered game that the window is being destroyed.
        pGame->OnWindowDestroy();
    }
    if (hWnd)
    {
        DestroyWindow(hWnd);
        hWnd = nullptr;
    }
}

int Window::GetClientWidth() const
{
    return ClientWidth;
}

int Window::GetClientHeight() const
{
    return ClientHeight;
}

bool Window::IsVSync() const
{
    return VSync;
}

void Window::SetVSync(bool vSync)
{
    VSync = vSync;
}

void Window::ToggleVSync()
{
    SetVSync(!VSync);
}

bool Window::IsFullScreen() const
{
    return Fullscreen;
}

// Set the fullscreen state of the window.
void Window::SetFullscreen(bool fullscreen)
{
    if (Fullscreen != fullscreen)
    {
        Fullscreen = fullscreen;

        if (Fullscreen) // Switching to fullscreen.
        {
            // Store the current window dimensions so they can be restored 
            // when switching out of fullscreen state.
            ::GetWindowRect(hWnd, &WindowRect);

            // Set the window style to a borderless window so the client area fills
            // the entire screen.
            UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

            ::SetWindowLongW(hWnd, GWL_STYLE, windowStyle);

            // Query the name of the nearest display device for the window.
            // This is required to set the fullscreen dimensions of the window
            // when using a multi-monitor setup.
            HMONITOR hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            ::GetMonitorInfo(hMonitor, &monitorInfo);

            ::SetWindowPos(hWnd, HWND_TOPMOST,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(hWnd, SW_MAXIMIZE);
        }
        else
        {
            // Restore all the window decorators.
            ::SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            ::SetWindowPos(hWnd, HWND_NOTOPMOST,
                WindowRect.left,
                WindowRect.top,
                WindowRect.right - WindowRect.left,
                WindowRect.bottom - WindowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(hWnd, SW_NORMAL);
        }
    }
}

void Window::ToggleFullscreen()
{
    SetFullscreen(!Fullscreen);
}


void Window::RegisterCallbacks(std::shared_ptr<Game> pGame)
{
    GamePtr = pGame;

    return;
}

void Window::OnUpdate(UpdateEventArgs&)
{
    //UpdateClock.Tick();

    if (auto pGame = GamePtr.lock())
    {
        FrameCounter++;

        UpdateEventArgs updateEventArgs(UpdateTicker.Update(), UpdateTicker.TimeSinceStart());
        pGame->OnUpdate(updateEventArgs);
    }
}

void Window::OnRender(RenderEventArgs&)
{
    //RenderClock.Tick();

    if (auto pGame = GamePtr.lock())
    {
        RenderEventArgs renderEventArgs(RenderTicker.Update(), RenderTicker.TimeSinceStart());
        pGame->OnRender(renderEventArgs);
    }
}

void Window::OnKeyPressed(KeyEventArgs& e)
{
    if (auto pGame = GamePtr.lock())
    {
        pGame->OnKeyPressed(e);
    }
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
    if (auto pGame = GamePtr.lock())
    {
        pGame->OnKeyReleased(e);
    }
}

// The mouse was moved
void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
    if (auto pGame = GamePtr.lock())
    {
        pGame->OnMouseMoved(e);
    }
}

// A button on the mouse was pressed
void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
    if (auto pGame = GamePtr.lock())
    {
        pGame->OnMouseButtonPressed(e);
    }
}

// A button on the mouse was released
void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
    if (auto pGame = GamePtr.lock())
    {
        pGame->OnMouseButtonReleased(e);
    }
}

// The mouse wheel was moved.
void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
    if (auto pGame = GamePtr.lock())
    {
        pGame->OnMouseWheel(e);
    }
}

void Window::OnResize(ResizeEventArgs& e)
{
    // Update the client size.
    if (ClientWidth != e.Width || ClientHeight != e.Height)
    {
        ClientWidth = max(1, e.Width);
        ClientHeight = max(1, e.Height);

        GDxDev->GetImmediateCommandQueue()->Flush();

        for (int i = 0; i < BufferCount; ++i)
        {
            D3D12BackBuffers[i].Reset();
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(DXGISwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(DXGISwapChain->ResizeBuffers(BufferCount, ClientWidth,
            ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        CurrentBackBufferIndex = DXGISwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews();
    }

    if (auto pGame = GamePtr.lock())
    {
        pGame->OnResize(e);
    }
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
    Application& app = Application::Get();

    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = ClientWidth;
    swapChainDesc.Height = ClientHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    ID3D12CommandQueue* pCommandQueue = GDxDev->GetImmediateCommandQueue()->DxCommandQueue.Get();

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        pCommandQueue,
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

    return dxgiSwapChain4;
}

// Update the render target views for the swapchain back buffers.
void Window::UpdateRenderTargetViews()
{
    auto device = GDxDev->DxDevice;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(D3D12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < BufferCount; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(DXGISwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

        D3D12BackBuffers[i] = backBuffer;

        rtvHandle.Offset(RTVDescriptorSize);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        CurrentBackBufferIndex, RTVDescriptorSize);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
    return D3D12BackBuffers[CurrentBackBufferIndex];
}

UINT Window::GetCurrentBackBufferIndex() const
{
    return CurrentBackBufferIndex;
}

UINT Window::Present()
{
    UINT syncInterval = VSync ? 1 : 0;
    UINT presentFlags = IsTearingSupported && !VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(DXGISwapChain->Present(syncInterval, presentFlags));
    CurrentBackBufferIndex = DXGISwapChain->GetCurrentBackBufferIndex();

    return CurrentBackBufferIndex;
}
};