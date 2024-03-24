#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_dx12.h"
//#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <span>
#include <SDL2/SDL.h>
#include <SDL_syswm.h>
#include <io.h>
#include <fcntl.h>
#include <chrono>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include <wrl.h>
#include <Shlwapi.h>
using Microsoft::WRL::ComPtr;

#include "DirectXMath.h"
using namespace DirectX;
#include <directx/d3dx12.h>
#include <d3dcompiler.h>

struct FrameContext
{
    ComPtr<ID3D12CommandAllocator> CommandAllocator;
    UINT64                  FenceValue;
};

// Data
static int const                    NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext                 g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT                         g_frameIndex = 0;

static int const                    NUM_BACK_BUFFERS = 3;
static ComPtr<ID3D12Device2> g_pd3dDevice = nullptr;
static ComPtr<ID3D12DescriptorHeap> g_pd3dRtvDescHeap = nullptr;
static ComPtr<ID3D12DescriptorHeap> g_pd3dSrvDescHeap = nullptr;
static ComPtr<ID3D12CommandQueue> g_pd3dCommandQueue = nullptr;
static ComPtr<ID3D12GraphicsCommandList> g_pd3dCommandList = nullptr;
static ComPtr<ID3D12Fence> g_fence = nullptr;
static HANDLE                       g_fenceEvent = nullptr;
static UINT64                       g_fenceLastSignaledValue = 0;
static ComPtr<IDXGISwapChain3> g_pSwapChain = nullptr;
static HANDLE                       g_hSwapChainWaitableObject = nullptr;
static ComPtr<ID3D12Resource> g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

static FrameContext FrameIndependentCtx = {};


struct VertexPosColor
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Color;
};

static VertexPosColor g_Vertices[8] = {
{ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
{ DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
{ DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
{ DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
{ DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
{ DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
{ DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
{ DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static ComPtr<ID3D12Resource> g_VertexBuffer;
static D3D12_VERTEX_BUFFER_VIEW g_VertexBufferView;
static ComPtr<ID3D12Resource> g_IndexBuffer;
static D3D12_INDEX_BUFFER_VIEW g_IndexBufferView;

static uint16_t g_Indicies[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};

static HWND g_hWnd = nullptr;

static ComPtr<ID3D12Resource> g_DepthBuffer;
static ComPtr<ID3D12DescriptorHeap> g_DSVHeap;

static ComPtr<ID3D12RootSignature> g_RootSignature;
static ComPtr<ID3D12PipelineState> g_PipelineState;

static float g_FoV = 45.0f;
static int g_Width = 1280;
static int g_Height = 720;

static DirectX::XMMATRIX g_ModelMatrix;
static DirectX::XMMATRIX g_ViewMatrix;
static DirectX::XMMATRIX g_ProjectionMatrix;
static D3D12_VIEWPORT g_Viewport;
static D3D12_RECT g_ScissorRect;

// Forward declarations of helper functions
bool CreateDeviceD3D();
void CleanupDeviceD3D();
void CreateSwapchainRTVDSV(bool resized);
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();

void CreateTriangleData();
void UploadToBuffer(ID3D12GraphicsCommandList* cmd, ID3D12Resource* dest, ID3D12Resource** intermediateBuf, size_t size, void* data);

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

void CreateConsole()
{
    if (!AllocConsole()) {
        // Add some error handling here.
        // You can call GetLastError() to get more info about the error.
        return;
    }

    // std::cout, std::clog, std::cerr, std::cin
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();

    // std::wcout, std::wclog, std::wcerr, std::wcin
    HANDLE hConOut = CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hConIn = CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
    SetStdHandle(STD_ERROR_HANDLE, hConOut);
    SetStdHandle(STD_INPUT_HANDLE, hConIn);
    std::wcout.clear();
    std::wclog.clear();
    std::wcerr.clear();
    std::wcin.clear();
}

void UIUpdate(ImGuiIO& io, bool& showDemoWindow, bool& showAnotherWindow, ImVec4& clearCol)
{

    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (showDemoWindow)
        ImGui::ShowDemoWindow(&showDemoWindow);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &showDemoWindow);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &showAnotherWindow);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clearCol); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (showAnotherWindow)
    {
        ImGui::Begin("Another Window", &showAnotherWindow);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            showAnotherWindow = false;
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
}

void InitGame()
{
}

void UpdateGame(float deltaTime)
{
    static float angle = static_cast<float>(0.0f);
    angle += 90.0f * 0.016f;
    const DirectX::XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
    g_ModelMatrix = DirectX::XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));
    const DirectX::XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
    const DirectX::XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
    const DirectX::XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
    g_ViewMatrix = DirectX::XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);
    float aspectRatio = static_cast<float>(g_Width) / static_cast<float>(g_Height);
    g_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(XMConvertToRadians(g_FoV), aspectRatio, 0.1f, 100.0f);
}

void RenderGame()
{
    

    g_pd3dCommandList->SetPipelineState(g_PipelineState.Get());
    g_pd3dCommandList->SetGraphicsRootSignature(g_RootSignature.Get());
    g_pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pd3dCommandList->IASetVertexBuffers(0, 1, &g_VertexBufferView);
    g_pd3dCommandList->IASetIndexBuffer(&g_IndexBufferView);

    DirectX::XMMATRIX mvpMatrix = XMMatrixMultiply(g_ModelMatrix, g_ViewMatrix);
    mvpMatrix = XMMatrixMultiply(mvpMatrix, g_ProjectionMatrix);
    g_pd3dCommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);

    g_pd3dCommandList->RSSetViewports(1, &g_Viewport);
    g_pd3dCommandList->RSSetScissorRects(1, &g_ScissorRect);
    
    D3D12_CPU_DESCRIPTOR_HANDLE dsDescriptor = g_DSVHeap->GetCPUDescriptorHandleForHeapStart();
    g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[g_pSwapChain->GetCurrentBackBufferIndex()], FALSE, &dsDescriptor);
    g_pd3dCommandList->DrawIndexedInstanced(_countof(g_Indicies), 1, 0, 0, 0);
}

void Render(ImGuiIO& io, bool& showDemoWindow, bool& showAnotherWindow, ImVec4& clearCol)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    UIUpdate(io, showDemoWindow, showAnotherWindow, clearCol);

    FrameContext* frameCtx = WaitForNextFrameResources();
    UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
    frameCtx->CommandAllocator->Reset();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_pd3dCommandList->Reset(frameCtx->CommandAllocator.Get(), nullptr);
    g_pd3dCommandList->ResourceBarrier(1, &barrier);


    // Render Dear ImGui graphics
    const float clear_color_with_alpha[4] = { clearCol.x * clearCol.w, clearCol.y * clearCol.w, clearCol.z * clearCol.w, clearCol.w };
    g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
    g_pd3dCommandList->ClearDepthStencilView(g_DSVHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    RenderGame();

    /*g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);

    ID3D12DescriptorHeap* descHeaps[] = {g_pd3dSrvDescHeap.Get()};

    g_pd3dCommandList->SetDescriptorHeaps(1, descHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList.Get());*/
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_pd3dCommandList->ResourceBarrier(1, &barrier);
    g_pd3dCommandList->Close();

    ID3D12CommandList* ppCommandLists[] = { g_pd3dCommandList.Get() };

    g_pd3dCommandQueue->ExecuteCommandLists(1, ppCommandLists);

    g_pSwapChain->Present(1, 0); // Present with vsync
    //g_pSwapChain->Present(0, 0); // Present without vsync

    UINT64 fenceValue = g_fenceLastSignaledValue + 1;
    g_pd3dCommandQueue->Signal(g_fence.Get(), fenceValue);
    g_fenceLastSignaledValue = fenceValue;
    frameCtx->FenceValue = fenceValue;
}

// Main code
int main(int argv, char** args)
{
    // Set working directory to executable directory
    {
        WCHAR path[MAX_PATH];
        HMODULE hModule = GetModuleHandleW(NULL);
        if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
        {
            PathRemoveFileSpecW(path);
            SetCurrentDirectoryW(path);
        }
    }
    g_ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    g_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(1280), static_cast<float>(720));
    g_FoV = 45.0;
    
    CreateConsole();
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to the latest version of SDL is recommended!)
    
    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("DX12 Playground", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_Width, g_Height, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = (HWND)wmInfo.info.win.window;
    g_hWnd = hwnd;
    // Initialize Direct3D
    if (!CreateDeviceD3D())
    {
        CleanupDeviceD3D();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForD3D(window);
    ImGui_ImplDX12_Init(g_pd3dDevice.Get(), NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap.Get(),
        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    InitGame();
    CreateTriangleData();

    // Main loop
    bool done = false;
    
    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();

    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event sdlEvent;
        while (SDL_PollEvent(&sdlEvent))
        {
            ImGui_ImplSDL2_ProcessEvent(&sdlEvent);
            if (sdlEvent.type == SDL_QUIT)
                done = true;
            if (sdlEvent.type == SDL_WINDOWEVENT && sdlEvent.window.event == SDL_WINDOWEVENT_CLOSE && sdlEvent.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (sdlEvent.type == SDL_WINDOWEVENT && sdlEvent.window.event == SDL_WINDOWEVENT_RESIZED && sdlEvent.window.windowID == SDL_GetWindowID(window))
            {
                g_Width = sdlEvent.window.data1;
                g_Height = sdlEvent.window.data2;
                // Release all outstanding references to the swap chain's buffers before resizing.
                CleanupRenderTarget();
                CreateSwapchainRTVDSV(true);
            }
        }

        UpdateGame(io.DeltaTime);

        Render(io, show_demo_window, show_another_window, clear_color);
    }


    WaitForLastSubmittedFrame();

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

// Helper functions

bool CreateDeviceD3D()
{
    // [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12Debug> pdx12Debug = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
    if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

    // [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    if (pdx12Debug != nullptr)
    {
        ComPtr<ID3D12InfoQueue> pInfoQueue = nullptr;
        g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
    }
#endif

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&FrameIndependentCtx.CommandAllocator)) != S_OK)
        return false;

    if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameIndependentCtx.CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (g_fenceEvent == nullptr)
        return false;

   

    CreateSwapchainRTVDSV(false);
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->SetFullscreenState(false, nullptr); g_pSwapChain = nullptr; }
    if (g_hSwapChainWaitableObject != nullptr) { CloseHandle(g_hSwapChainWaitableObject); }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        g_frameContext[i].CommandAllocator = nullptr;
    FrameIndependentCtx.CommandAllocator = nullptr;
    g_VertexBuffer = nullptr;
    g_IndexBuffer = nullptr;
    g_DepthBuffer = nullptr;
    g_DSVHeap = nullptr;
    g_RootSignature = nullptr;
    g_PipelineState = nullptr;
    g_pd3dCommandQueue = nullptr;
    g_pd3dCommandList = nullptr;
    g_pd3dRtvDescHeap = nullptr;
    g_pd3dSrvDescHeap = nullptr;
    g_fence = nullptr;
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
    g_pd3dDevice = nullptr;



#ifdef DX12_ENABLE_DEBUG_LAYER
    
    if (ComPtr<IDXGIDebug1> pDebug = nullptr; SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
    }
#endif
}

void CreateSwapchainRTVDSV(bool resized)
{
    if(!resized){
        // Setup swap chain
        DXGI_SWAP_CHAIN_DESC1 sd;
        {
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = NUM_BACK_BUFFERS;
            sd.Width = g_Width;
            sd.Height = g_Height;
            sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            sd.Scaling = DXGI_SCALING_STRETCH;
            sd.Stereo = FALSE;
        }
        ComPtr<IDXGIFactory4> dxgiFactory = nullptr;
        ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));
        ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue.Get(), g_hWnd, &sd, nullptr, nullptr, &swapChain1));
        ThrowIfFailed(swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)));
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    }
    else
    {
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
            g_mainRenderTargetResource[i] = nullptr;
        g_DepthBuffer = nullptr;
        g_DSVHeap = nullptr;
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(g_pSwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(g_pSwapChain->ResizeBuffers(NUM_BACK_BUFFERS, g_Width, g_Height,
            swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
    }

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_mainRenderTargetResource[i]));
        g_pd3dDevice->CreateRenderTargetView(g_mainRenderTargetResource[i].Get(), nullptr, g_mainRenderTargetDescriptor[i]);
    }
    // Create the depth buffer
    CD3DX12_RESOURCE_DESC depthTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, g_Width, g_Height);
    depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    auto dsvHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    g_pd3dDevice->CreateCommittedResource(
        &dsvHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &depthTexDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&g_DepthBuffer));


    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    g_pd3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_DSVHeap));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
    dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvViewDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvViewDesc.Texture2D.MipSlice = 0;

    g_pd3dDevice->CreateDepthStencilView(
        g_DepthBuffer.Get(),
        &dsvViewDesc,
        g_DSVHeap->GetCPUDescriptorHandleForHeapStart());
}

void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        g_mainRenderTargetResource[i] = nullptr;
    g_DepthBuffer = nullptr;
    g_DSVHeap = nullptr;
}

void WaitForLastSubmittedFrame()
{
    FrameContext* frameCtx = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext* WaitForNextFrameResources()
{
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = { g_hSwapChainWaitableObject, nullptr };
    DWORD numWaitableObjects = 1;

    FrameContext* frameCtx = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtx->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtx;
}

void CreateTriangleData()
{
    

    auto bufResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(g_Vertices));
    auto bufHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    g_pd3dDevice->CreateCommittedResource(
		&bufHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&bufResDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&g_VertexBuffer));
    
    bufResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(g_Indicies));

    g_pd3dDevice->CreateCommittedResource(
        &bufHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &bufResDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&g_IndexBuffer));

    ComPtr<ID3D12Resource> IntermediateVertexBuffer;
    ComPtr<ID3D12Resource> IntermediateIndexBuffer;

    g_pd3dCommandList->Reset(FrameIndependentCtx.CommandAllocator.Get(), nullptr);

    UploadToBuffer(g_pd3dCommandList.Get(), g_VertexBuffer.Get(), &IntermediateVertexBuffer, sizeof(g_Vertices), g_Vertices);
    UploadToBuffer(g_pd3dCommandList.Get(), g_IndexBuffer.Get(), &IntermediateIndexBuffer, sizeof(g_Indicies), g_Indicies);

    // Create the vertex buffer view
    g_VertexBufferView.BufferLocation = g_VertexBuffer->GetGPUVirtualAddress();
    g_VertexBufferView.StrideInBytes = sizeof(VertexPosColor);
    g_VertexBufferView.SizeInBytes = sizeof(g_Vertices);
    
    // Create the index buffer view
    g_IndexBufferView.BufferLocation = g_IndexBuffer->GetGPUVirtualAddress();
    g_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    g_IndexBufferView.SizeInBytes = sizeof(g_Indicies);

    // Create Root Signature

    CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};
    rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

    rootSignatureDesc.Init_1_1(
		1,
		rootParameters,
		0,
		nullptr,
        rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    g_pd3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_RootSignature));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = g_RootSignature.Get();
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout)};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"Triangle.vs.cso", &vertexShaderBlob));
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"Triangle.ps.cso", &pixelShaderBlob));

	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());

    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineStateStream.RTVFormats = rtvFormats;

    CD3DX12_RASTERIZER_DESC desc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.CullMode = D3D12_CULL_MODE_NONE;
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    desc.DepthClipEnable = TRUE;
    pipelineStateStream.Rasterizer = desc;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

    g_pd3dDevice->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&g_PipelineState));

    //Execute and flush
    g_pd3dCommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { g_pd3dCommandList.Get() };
    g_pd3dCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ComPtr<ID3D12Fence> fence;
    g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    g_pd3dCommandQueue->Signal(fence.Get(), 1);
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    FrameIndependentCtx.CommandAllocator->Reset();
}

void UploadToBuffer(ID3D12GraphicsCommandList* cmd, ID3D12Resource* dest, ID3D12Resource** intermediateBuf, size_t size, void* data)
{
    // Create the intermediate upload heap
    auto intermediateHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto intermediateResDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    g_pd3dDevice->CreateCommittedResource(
		&intermediateHeapProp,
		D3D12_HEAP_FLAG_NONE,
        &intermediateResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(intermediateBuf));

    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = data;
    subresourceData.RowPitch = size;
    subresourceData.SlicePitch = subresourceData.RowPitch;

    UpdateSubresources(cmd, dest, *intermediateBuf, 0, 0, 1, &subresourceData);
}