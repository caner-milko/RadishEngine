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
#include <filesystem>
#include <variant>

#include "DXHelpers.h"
#include <Shlwapi.h>

#include <tiny_obj_loader.h>
#include <stb_image.h>

#include "RendererCommon.h"
#include "Pipelines/StaticMeshPipeline.h"
#include "Pipelines/GenerateMipsPipeline.h"
#include "ShaderManager.h"

namespace dxpg
{

// Data
static int const                    NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext                 g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT                         g_frameIndex = 0;

static int const                    NUM_BACK_BUFFERS = 3;
static ComPtr<ID3D12Device2> g_pd3dDevice = nullptr;

static ComPtr<ID3D12CommandQueue> g_pd3dCommandQueue = nullptr;
static ComPtr<ID3D12GraphicsCommandList> g_pd3dCommandList = nullptr;
static ComPtr<ID3D12Fence> g_fence = nullptr;
static HANDLE                       g_fenceEvent = nullptr;
static UINT64                       g_fenceLastSignaledValue = 0;
static ComPtr<IDXGISwapChain3> g_pSwapChain = nullptr;
static HANDLE                       g_hSwapChainWaitableObject = nullptr;
static ComPtr<ID3D12Resource> g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static std::unique_ptr<dx12::RenderTargetView> g_mainRTV[NUM_BACK_BUFFERS] = {};
static std::unique_ptr<dx12::RenderTargetView> g_mainRTVSRGB[NUM_BACK_BUFFERS] = {};

static FrameContext FrameIndependentCtx = {};

struct Material
{
    std::string Name;
    std::optional<std::string> DiffuseTextureName;
    std::optional<std::string> AlphaTextureName;

    Vector3 DiffuseColor = { 1, 1, 1 };

    std::shared_ptr<dx12::D3D12Material> GPUMaterial;
};

struct MeshAsset
{
	std::vector<tinyobj::index_t> Indicies;
    std::string Name;
    std::shared_ptr<dxpg::dx12::D3D12Mesh> GPUMesh;
	std::string Material;
};

struct MeshGroup
{
    std::vector<float> Positions;
    std::vector<float> Normals;
    std::vector<float> TexCoords;
    std::unique_ptr<dx12::VertexData> GPUVertexData;
    std::vector<MeshAsset> Meshes;
    std::unordered_map<std::string, Material> Materials;
};

std::vector<MeshGroup> g_LoadedMeshGroups;
std::unordered_map<std::string, std::unique_ptr<dx12::D3D12Texture>> g_LoadedTextures;

static HWND g_hWnd = nullptr;

static ComPtr<ID3D12Resource> g_DepthBuffer;
static std::unique_ptr<dx12::DepthStencilView> g_DSV;

StaticMeshPipeline g_StaticMeshPipeline;
GenerateMipsPipeline g_GenerateMipsPipeline;

static int g_Width = 1280;
static int g_Height = 720;

struct IO
{
    // Keep last & current sdl key states
    enum KEY_STATE
    {
        KEY_UP = 0,
	    KEY_DOWN = 1,
        KEY_PRESSED = 2,
        KEY_RELEASED = 3
    };
    KEY_STATE CUR_KEYS[SDL_NUM_SCANCODES];
    
    bool CursorEnabled = false;

    struct Immediate
    {
        float MouseWheelDelta = 0.0f;
        Vector2 MouseDelta = { 0, 0 };
    } Immediate;

    bool IsKeyDown(SDL_Scancode key)
    {
	    return CUR_KEYS[key] == KEY_DOWN || CUR_KEYS[key] == KEY_PRESSED;
    }

    bool IsKeyPressed(SDL_Scancode key)
    {
	    return CUR_KEYS[key] == KEY_PRESSED;
    }

    bool IsKeyReleased(SDL_Scancode key)
    {
	    return CUR_KEYS[key] == KEY_RELEASED;
    }

    bool IsKeyUp(SDL_Scancode key)
    {
	    return CUR_KEYS[key] == KEY_UP || CUR_KEYS[key] == KEY_RELEASED;
    }

} static g_IO;



static D3D12_VIEWPORT g_Viewport;
static D3D12_RECT g_ScissorRect;

struct Camera
{
    float MoveSpeed = 1000.0f;
    float RotSpeed = 2.0f;
    Vector4 Position = { 0, 0, -10, 1 };
    Vector4 Rotation = { 0, 0, 0, 0 };
    float FoV = 45.0f;
    
	Matrix4x4 GetRotationMatrix()
	{
		return DirectX::XMMatrixRotationRollPitchYawFromVector(Rotation);
	}

	Vector4 GetDirection()
	{
		return DirectX::XMVector4Transform(DirectX::XMVectorSet(0, 0, 1, 0), GetRotationMatrix());
	}
	Vector4 GetRight()
	{
		return DirectX::XMVector4Transform(DirectX::XMVectorSet(1, 0, 0, 0), GetRotationMatrix());
	}
	Vector4 GetUp()
	{
		return DirectX::XMVector4Transform(DirectX::XMVectorSet(0, 1, 0, 0), GetRotationMatrix());
	}

    void SetFoV(float fov)
    {
		FoV = std::clamp(fov, 30.0f, 90.0f);
	}

	Matrix4x4 GetViewMatrix()
	{
		const Vector4 upDirection = { 0, 1, 0, 0 };
		return DirectX::XMMatrixLookToLH(Position, GetDirection(), upDirection);
	}

	Matrix4x4 GetProjectionMatrix()
	{
		float aspectRatio = static_cast<float>(g_Width) / static_cast<float>(g_Height);
		return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(FoV), aspectRatio, 0.1f, 10000.0f);
	}

    ViewData ToViewData()
    {
		auto view = GetViewMatrix();
		auto proj = GetProjectionMatrix();
		return { view, proj, XMMatrixMultiply(view, proj), Position, GetDirection(), FoV };
    }

} g_Cam = {};
// Forward declarations of helper functions
bool CreateDeviceD3D();
void CleanupDeviceD3D();
void CreateSwapchainRTVDSV(bool resized);
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();

void LoadSceneData();
void UploadToBuffer(ID3D12GraphicsCommandList* cmd, ID3D12Resource* dest, ID3D12Resource** intermediateBuf, size_t size, void* data);
void UploadToTexture(ID3D12GraphicsCommandList* cmd, ID3D12Resource* dest, ID3D12Resource** intermediateBuf, size_t width, size_t height, size_t componentCount, void* data);

MeshGroup* LoadObjFile(const char* path);

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

void BeginFrame(FrameContext& frameCtx)
{
    frameCtx.GPUHeapPages[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = dx12::g_GPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->AllocatePage();
    frameCtx.GPUHeapPages[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = dx12::g_GPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->AllocatePage();
    g_pd3dCommandList->Reset(frameCtx.CommandAllocator.Get(), nullptr);
    frameCtx.Ready = true;
}

void EndFrame(FrameContext& frameCtx)
{
    frameCtx.Ready = false;
}

void ClearFrame(FrameContext& frameCtx)
{
    for (auto& [type, heapPage] : frameCtx.GPUHeapPages)
    {
        dx12::g_GPUDescriptorAllocator->Heaps[type]->FreePage(heapPage);
    }
	frameCtx.GPUHeapPages.clear();
    frameCtx.CommandAllocator->Reset();
}

void UIUpdate(ImGuiIO& io, bool& showDemoWindow, bool& showAnotherWindow, ImVec4& clearCol)
{

    ImGui::NewFrame();

    {
        ImGui::Begin("Camera");                          // Create a window called "Hello, world!" and append into it.

        ImGui::InputFloat3("Position", &g_Cam.Position.m128_f32[0], "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat3("Rotation", &g_Cam.Rotation.m128_f32[0], "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat("FoV", &g_Cam.FoV, 0.1f, 1.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

        ImGui::SliderFloat("Move Speed", &g_Cam.MoveSpeed, 0.0f, 10000.0f);
        ImGui::SliderFloat("Rotation Speed", &g_Cam.RotSpeed, 0.1f, 10.0f);
        
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
    memset(g_IO.CUR_KEYS, 0, sizeof(g_IO.CUR_KEYS));
}

void UpdateGame(float deltaTime)
{
    if (g_IO.IsKeyPressed(SDL_SCANCODE_E))
    {
        SDL_SetRelativeMouseMode((SDL_bool)g_IO.CursorEnabled);
        g_IO.CursorEnabled = !g_IO.CursorEnabled;
    }
    if (g_IO.IsKeyPressed(SDL_SCANCODE_R))
    {
        g_Cam = {};
    }
    g_Cam.SetFoV(g_Cam.FoV - g_IO.Immediate.MouseWheelDelta * 2.0f);
    Vector4 moveDir = { int32_t(g_IO.IsKeyDown(SDL_SCANCODE_D)) - int32_t(g_IO.IsKeyDown(SDL_SCANCODE_A))
        , int32_t(g_IO.IsKeyDown(SDL_SCANCODE_SPACE)) - int32_t(g_IO.IsKeyDown(SDL_SCANCODE_LCTRL))
        , int32_t(g_IO.IsKeyDown(SDL_SCANCODE_W)) - int32_t(g_IO.IsKeyDown(SDL_SCANCODE_S)), 0 };
    moveDir = XMVector4Normalize(moveDir);
        
    g_Cam.Position = g_Cam.Position + XMVector4Transform(moveDir, g_Cam.GetRotationMatrix()) * deltaTime * g_Cam.MoveSpeed;
        
    if(!g_IO.CursorEnabled)
    {
        g_Cam.Rotation = g_Cam.Rotation + XMVectorSet(g_IO.Immediate.MouseDelta.y, g_IO.Immediate.MouseDelta.x, 0, 0) * g_Cam.RotSpeed * deltaTime;
        g_Cam.Rotation = XMVectorSetX(g_Cam.Rotation, std::clamp(XMVectorGetX(g_Cam.Rotation), -XM_PIDIV2 + 0.0001f, XM_PIDIV2 - 0.0001f));
    }
}

void Render(ImGuiIO& io, bool& showDemoWindow, bool& showAnotherWindow, ImVec4& clearCol)
{
    FrameContext* frameCtx = WaitForNextFrameResources();
    BeginFrame(*frameCtx);
    UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_pd3dCommandList->ResourceBarrier(1, &barrier);

    // Render Dear ImGui graphics
    const float clear_color_with_alpha[4] = { clearCol.x * clearCol.w, clearCol.y * clearCol.w, clearCol.z * clearCol.w, clearCol.w };
    g_pd3dCommandList->ClearRenderTargetView(g_mainRTVSRGB[backBufferIdx]->GetCPUHandle(), clear_color_with_alpha, 0, nullptr);
    g_pd3dCommandList->ClearDepthStencilView(g_DSV->GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    auto heaps = dx12::g_GPUDescriptorAllocator->GetHeaps();

    g_pd3dCommandList->SetDescriptorHeaps(heaps.size(), heaps.data());

    g_pd3dCommandList->SetPipelineState(g_StaticMeshPipeline.PipelineState.DXPipelineState.Get());
    g_pd3dCommandList->SetGraphicsRootSignature(g_StaticMeshPipeline.RootSignature.DXSignature.Get());
    g_pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pd3dCommandList->RSSetViewports(1, &g_Viewport);
    g_pd3dCommandList->RSSetScissorRects(1, &g_ScissorRect);

    auto rtvHandles = g_mainRTVSRGB[g_pSwapChain->GetCurrentBackBufferIndex()]->GetCPUHandle();
    auto dsvHandle = g_DSV->GetCPUHandle();

    g_pd3dCommandList->OMSetRenderTargets(1, &rtvHandles, FALSE, &dsvHandle);
    
    SceneDataView sceneDataView{};

    for (auto& group : g_LoadedMeshGroups)
    {
		auto& groupView = sceneDataView.MeshGroups.emplace_back();

		groupView.VertexSRV = frameCtx->GetGPUAllocation(group.GPUVertexData->VertexSRV.get())->GetGPUHandle();
        for (auto& mesh : group.Meshes)
        {
            auto& meshView = groupView.Meshes.emplace_back();
            meshView.IndexCount = mesh.Indicies.size();
            meshView.IndexBufferView = mesh.GPUMesh->IndicesView;
            meshView.ModelMatrix = mesh.GPUMesh->GetWorldMatrix();
            auto& mat = group.Materials[mesh.Material];

            if (mat.GPUMaterial->DiffuseSRV)
            {
                meshView.UseDiffuseTexture = true;
                meshView.DiffuseSRV = frameCtx->GetGPUAllocation(mat.GPUMaterial->DiffuseSRV)->GetGPUHandle();
            }
            meshView.DiffuseColor = mat.DiffuseColor;
        }
    }

	g_StaticMeshPipeline.Run(g_pd3dCommandList.Get(), g_Cam.ToViewData(), sceneDataView, *frameCtx);

    // ImGui
    {
        auto rtvHandle = g_mainRTV[backBufferIdx]->GetCPUHandle();

        g_pd3dCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList.Get());
    }
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
    EndFrame(*frameCtx);
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

    dx12::ShaderManager::Create();

    {
        dx12::g_CPUDescriptorAllocator = dx12::CPUDescriptorHeapAllocator::Create(g_pd3dDevice.Get());
		dx12::g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024);
		dx12::g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024);
		dx12::g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024);
		dx12::g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024);

        dx12::g_GPUDescriptorAllocator = dx12::GPUDescriptorHeapAllocator::Create(g_pd3dDevice.Get());
		dx12::g_GPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048*4, NUM_BACK_BUFFERS, 1);
		dx12::g_GPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024, NUM_BACK_BUFFERS);
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
    g_LoadedMeshGroups.clear();
    g_LoadedTextures.clear();
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->SetFullscreenState(false, nullptr); g_pSwapChain = nullptr; }
    if (g_hSwapChainWaitableObject != nullptr) { CloseHandle(g_hSwapChainWaitableObject); }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        g_frameContext[i].CommandAllocator = nullptr;
    FrameIndependentCtx.CommandAllocator = nullptr;
    g_DepthBuffer = nullptr;
    g_StaticMeshPipeline = {};
    g_pd3dCommandQueue = nullptr;
    g_pd3dCommandList = nullptr;
    g_fence = nullptr;
    dx12::g_CPUDescriptorAllocator = nullptr;
    dx12::g_GPUDescriptorAllocator = nullptr;
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
	dx12::ShaderManager::Destroy();
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

        dx12::g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->StaticPage->Reset();
        
        g_DepthBuffer = nullptr;
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(g_pSwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(g_pSwapChain->ResizeBuffers(NUM_BACK_BUFFERS, g_Width, g_Height,
            swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
    }

    dx12::g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->StaticPage->Reset();
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_mainRenderTargetResource[i]));
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
        g_mainRTV[i] = dx12::RenderTargetView::Create(g_pd3dDevice.Get(), { dx12::ResourceViewToDesc<dx12::ViewTypes::RenderTargetView>{.Desc = &rtvDesc, .Resource = g_mainRenderTargetResource[i].Get()} });
		auto srgbDesc = rtvDesc;
		srgbDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		g_mainRTVSRGB[i] = dx12::RenderTargetView::Create(g_pd3dDevice.Get(), { dx12::ResourceViewToDesc<dx12::ViewTypes::RenderTargetView>{.Desc = &srgbDesc, .Resource = g_mainRenderTargetResource[i].Get()} });
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

    dx12::g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->StaticPage->Reset();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
    dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvViewDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvViewDesc.Texture2D.MipSlice = 0;
    g_DSV = dx12::DepthStencilView::Create(g_pd3dDevice.Get(), { dx12::ResourceViewToDesc<dx12::ViewTypes::DepthStencilView>{.Desc = &dsvViewDesc, .Resource = g_DepthBuffer.Get()} });
}

void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        g_mainRenderTargetResource[i] = nullptr;
    g_DepthBuffer = nullptr;

    dx12::g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->StaticPage->Reset();
    dx12::g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->StaticPage->Reset();
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

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        ClearFrame(g_frameContext[i]);
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
    ClearFrame(*frameCtx);
    
    return frameCtx;
}

void LoadSceneData()
{
	g_StaticMeshPipeline.Setup(g_pd3dDevice.Get());

    ClearFrame(FrameIndependentCtx);
    BeginFrame(FrameIndependentCtx);

    auto group = LoadObjFile(DXPG_SPONZA_DIR "sponza.obj");
    auto& vertexData = (group->GPUVertexData = dx12::VertexData::Create(g_pd3dDevice.Get(), group->Positions.size() / 3, group->Normals.size() / 3, group->TexCoords.size() / 2));

    const auto createAndUploadBuf = [](ComPtr<ID3D12Resource>& buf, ComPtr<ID3D12Resource>& uploadBuf, D3D12_HEAP_PROPERTIES heapProps, void* data, size_t size)
        {
            auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
            g_pd3dDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&buf));
            UploadToBuffer(g_pd3dCommandList.Get(), buf.Get(), &uploadBuf, size, data);
        };

    std::vector<ComPtr<ID3D12Resource>> intermediateBuffers;

    UploadToBuffer(g_pd3dCommandList.Get(), vertexData->PositionsBuffer.Get(), &intermediateBuffers.emplace_back(), group->Positions.size() * 3 * sizeof(float), group->Positions.data());
    UploadToBuffer(g_pd3dCommandList.Get(), vertexData->NormalsBuffer.Get(), &intermediateBuffers.emplace_back(), group->Normals.size() * 3 * sizeof(float), group->Normals.data());
    UploadToBuffer(g_pd3dCommandList.Get(), vertexData->TexCoordsBuffer.Get(), &intermediateBuffers.emplace_back(), group->TexCoords.size() * 2 * sizeof(float), group->TexCoords.data());

    for (auto& mesh : group->Meshes)
    {
        mesh.GPUMesh = dx12::D3D12Mesh::Create(g_pd3dDevice.Get(), group->GPUVertexData.get(), mesh.Indicies.size(), sizeof(tinyobj::index_t));
        UploadToBuffer(g_pd3dCommandList.Get(), mesh.GPUMesh->Indices.Get(), &intermediateBuffers.emplace_back(), mesh.Indicies.size() * sizeof(tinyobj::index_t), mesh.Indicies.data());
    }

    for (auto& [_, mat] : group->Materials)
    {
        auto gpuMat = mat.GPUMaterial = std::make_shared<dx12::D3D12Material>();

        constexpr auto loadTex = [](dx12::ShaderResourceView*& srvToFill, std::optional<std::string> const& textureName, bool alphaOnly, decltype(intermediateBuffers)& intermediateBufs)
            {
                if (textureName)
                {
                    if (auto it = g_LoadedTextures.find(*textureName); it != g_LoadedTextures.end())
                    {
                        srvToFill = it->second->SRV.get();
                    }
                    else
                    {
                        // Load the texture
                        int width, height, channels;
                        stbi_set_flip_vertically_on_load(true);
                        auto data = stbi_load(textureName->c_str(), &width, &height, &channels, alphaOnly ? STBI_grey : STBI_rgb_alpha);
                        if (!data)
                            return;

                        // Create the texture
                        auto& tex = g_LoadedTextures[*textureName] = dx12::D3D12Texture::Create(g_pd3dDevice.Get(), alphaOnly ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height);

                        //Copy the texture data
                        auto intermediateHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                        auto intermediateResDesc = CD3DX12_RESOURCE_DESC::Buffer(width * height * (alphaOnly ? 1 : 4));

                        UploadToTexture(g_pd3dCommandList.Get(), tex->Resource.Get(), &intermediateBufs.emplace_back(), width, height, 4, data);

                        // transition to shader resource
                        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                        g_pd3dCommandList->ResourceBarrier(1, &barrier);

                        stbi_image_free(data);

                        srvToFill = tex->SRV.get();
                    }
                }
            };

		loadTex(gpuMat->DiffuseSRV, mat.DiffuseTextureName, false, intermediateBuffers);
		loadTex(gpuMat->AlphaSRV, mat.AlphaTextureName, true, intermediateBuffers);
    }


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
	CloseHandle(fenceEvent);
    EndFrame(FrameIndependentCtx);
    ClearFrame(FrameIndependentCtx);
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


void UploadToTexture(ID3D12GraphicsCommandList* cmd, ID3D12Resource* dest, ID3D12Resource** intermediateBuf, size_t width, size_t height, size_t componentCount, void* data)
{
    // Create the intermediate upload heap
    auto intermediateHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto intermediateResDesc = CD3DX12_RESOURCE_DESC::Buffer(width*height*componentCount);
    g_pd3dDevice->CreateCommittedResource(
        &intermediateHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &intermediateResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(intermediateBuf));

    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = data;
    subresourceData.RowPitch = width * componentCount;
    subresourceData.SlicePitch = height * subresourceData.RowPitch;

    UpdateSubresources(cmd, dest, *intermediateBuf, 0, 0, 1, &subresourceData);
}

MeshGroup* LoadObjFile(const char* path)
{
    // Load the obj file using tinyobjloader
    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader reader;
    reader.ParseFromFile(path, readerConfig);
    assert(reader.Valid());
    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    auto& meshGroup = g_LoadedMeshGroups.emplace_back();
    meshGroup.Positions = attrib.vertices;
    meshGroup.Normals = attrib.normals;
    meshGroup.TexCoords = attrib.texcoords;

    meshGroup.Meshes.reserve(shapes.size());

    for (auto& mat : reader.GetMaterials())
    {
        auto& material = meshGroup.Materials[mat.name];
        material.Name = mat.name;

        // Load the textures
        if (!mat.diffuse_texname.empty())
            material.DiffuseTextureName = std::filesystem::path(path).parent_path().string() + "/" + mat.diffuse_texname;
        else
			material.DiffuseColor = Vector3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);

		if (!mat.alpha_texname.empty())
			material.AlphaTextureName = std::filesystem::path(path).parent_path().string() + "/" + mat.alpha_texname;
    }

    // Loop over shapes
    for (auto& shape : shapes)
    {
        auto& mesh = meshGroup.Meshes.emplace_back();
        mesh.Indicies = shape.mesh.indices;
        mesh.Name = shape.name;

        if (!shape.mesh.material_ids.empty())
        {
			auto& mat = reader.GetMaterials()[shape.mesh.material_ids[0]];
			mesh.Material = mat.name;
        }
    }
    
    return &meshGroup;
}
}

// Main code
int main(int argv, char** args)
{
    using namespace::dxpg;
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
    g_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(g_Width), static_cast<float>(g_Height));

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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForD3D(window);
    auto fontAllocation = dx12::g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
    ImGui_ImplDX12_Init(g_pd3dDevice.Get(), NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, fontAllocation->Heap->Heap.Get(),
        fontAllocation->GetCPUHandle(),
        fontAllocation->GetGPUHandle());
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
    LoadSceneData();
    SDL_SetRelativeMouseMode(SDL_TRUE);
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
                g_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(g_Width), static_cast<float>(g_Height));
                // Release all outstanding references to the swap chain's buffers before resizing.
                CleanupRenderTarget();
                CreateSwapchainRTVDSV(true);
            }
            if (sdlEvent.type == SDL_KEYDOWN)
                g_IO.CUR_KEYS[sdlEvent.key.keysym.scancode] = sdlEvent.key.repeat ? IO::KEY_DOWN : IO::KEY_PRESSED;
            if (sdlEvent.type == SDL_KEYUP)
                g_IO.CUR_KEYS[sdlEvent.key.keysym.scancode] = IO::KEY_RELEASED;
            if (sdlEvent.type == SDL_MOUSEMOTION)
                g_IO.Immediate.MouseDelta = { float(sdlEvent.motion.xrel), float(sdlEvent.motion.yrel) };
            if (sdlEvent.type == SDL_MOUSEWHEEL)
                g_IO.Immediate.MouseWheelDelta = sdlEvent.wheel.y;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        UIUpdate(io, show_demo_window, show_another_window, clear_color);

        UpdateGame(io.DeltaTime);

        Render(io, show_demo_window, show_another_window, clear_color);

        for (int i = 0; i < 322; i++)
        {
            if (g_IO.CUR_KEYS[i] == IO::KEY_PRESSED)
                g_IO.CUR_KEYS[i] = IO::KEY_DOWN;
            if (g_IO.CUR_KEYS[i] == IO::KEY_RELEASED)
                g_IO.CUR_KEYS[i] = IO::KEY_UP;
        }

        g_IO.Immediate = {};
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