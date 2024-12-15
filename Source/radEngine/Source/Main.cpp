#include "imgui.h"
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

#include <Shlwapi.h>

#include "InputManager.h"

#include "Graphics/TextureManager.h"
#include "Graphics/ModelManager.h"

#include "Graphics/Renderer.h"
#include "ProcGen/TerrainGenerator.h"
#include "Systems.h"

extern "C" { __declspec(dllexport) extern const unsigned int D3D12SDKVersion = DIRECT3D_AGILITY_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace rad
{

// Data
static SDL_Window* g_SDLWindow = nullptr;
static HWND g_hWnd = nullptr;
static Renderer g_Renderer;
static entt::registry g_EnttRegistry;
static entt::entity g_Camera;
static entt::entity g_DirectionalLight;

static struct EnttSystems
{
	ecs::CCameraSystem CameraSystem{};
	ecs::CViewpointControllerSystem ViewpointControllerSystem{};
	ecs::CLightSystem LightSystem{};
	ecs::CStaticRenderSystem StaticRenderSystem{};
	ecs::CUISystem UISystem{};
} g_EnttSystems;

static int g_Width = 1920;
static int g_Height = 1080;

//proc::TerrainGenerator g_TerrainGenerator;
//struct TerrainRenderData
//{
//    rad::proc::TerrainData Terrain{};
//};
//
//std::unique_ptr<TerrainRenderData> Terrain;
//proc::ErosionParameters ErosionParams = {};



std::unordered_map<std::string, std::function<std::pair<rad::DXTexture*, rad::DescriptorAllocationView>()>> g_TextureSelections;
std::string g_SelectedTexture = "None";

void LoadSceneData();

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



entt::entity GetCamera()
{
	auto view = g_EnttRegistry.view<ecs::CCamera>();
	for (auto entity : view)
	{
		return entity;
	}
	return entt::null;
}


void UIUpdate(ImGuiIO& io, bool& showDemoWindow, bool& showAnotherWindow, ImVec4& clearCol)
{

    ImGui::NewFrame();


    // Rendering
    ImGui::Render();
}

void InitGame()
{
	InputManager::Create();
	InputManager::Get().Init();
	g_EnttSystems = {};
	g_EnttSystems.StaticRenderSystem.Init(g_Renderer);
	g_EnttSystems.UISystem.Init(g_Renderer, g_SDLWindow);

	g_Camera = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(g_Camera, "Camera");
	auto& camSceneTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(g_Camera, g_Camera);
	g_EnttRegistry.emplace<ecs::CCamera>(g_Camera);
	auto& viewpoint =  g_EnttRegistry.emplace<ecs::CViewpoint>(g_Camera, ecs::CViewpoint{ .Projection = ecs::CViewpoint::Perspective{.Fov = 60.0f, 
		.Near = 0.1f, .Far = 1000.0f, .AspectRatio = 16.0f / 9.0f, }});
	ecs::Transform camTransform{};
	camTransform.Position = { 5.3f, 2.f, -1.2f };
	camTransform.Rotation = { 0.15f, -1.348f, 0.f };
	camSceneTransform.SetTransform(camTransform);
	auto& controller = g_EnttRegistry.emplace<ecs::CViewpointController>(g_Camera, ecs::CViewpointController(camSceneTransform.GetWorldTransform(), viewpoint));

	g_DirectionalLight = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(g_DirectionalLight, "DirectionalLight");
	auto& lightSceneTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(g_DirectionalLight, g_DirectionalLight);
	ecs::Transform lightTransform{};
	lightTransform.Position = { 10.0f, 24.5f, -3.5f };
	lightTransform.Rotation = { 1.0f, -1.2f, 0.f };
	lightSceneTransform.SetTransform(lightTransform);
	g_EnttRegistry.emplace<ecs::CLight>(g_DirectionalLight);
	auto& lightViewpoint = g_EnttRegistry.emplace<ecs::CViewpoint>(g_DirectionalLight, ecs::CViewpoint{ .Projection = ecs::CViewpoint::Orthographic{.Width = 50.0f, .Height = 50.0f} });
	g_EnttRegistry.emplace<ecs::CViewpointController>(g_DirectionalLight, ecs::CViewpointController(lightSceneTransform.GetWorldTransform(), lightViewpoint));
}

void UpdateGame(float deltaTime, RenderFrameRecord& frameRecord)
{
	g_EnttSystems.ViewpointControllerSystem.Update(g_EnttRegistry, InputManager::Get(), deltaTime, g_Renderer);
	g_EnttSystems.CameraSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems.LightSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems.StaticRenderSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems.UISystem.Update(g_EnttRegistry, g_Renderer);
}

bool InitRenderer(HWND window, uint32_t width, uint32_t height)
{
	return g_Renderer.Initialize(
#ifdef NDEBUG
		false,
#else
		true,
#endif
		window, width, height
	);
}

void LoadSceneData()
{
	OptionalRef<ObjModel> sponzaObj{};
	g_Renderer.FrameIndependentCommand([&](CommandContext& commmandCtx)
		{
			sponzaObj = g_Renderer.ModelManager->LoadModel(RAD_SPONZA_DIR "sponza.obj", commmandCtx);
		});
	if (!sponzaObj)
	{
		std::cout << "Failed to load sponza model" << std::endl;
		return;
	}
	entt::entity sponzaRoot = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(sponzaRoot, "SponzaRoot");
	auto& rootTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(sponzaRoot, sponzaRoot);
	rootTransform.SetTransform(ecs::Transform{ .Scale = glm::vec3(0.01f) });
	for (auto& [name, meshInfo] : sponzaObj->Meshes)
	{
		entt::entity mesh = g_EnttRegistry.create();
		g_EnttRegistry.emplace<ecs::CEntityInfo>(mesh, name);
		auto& meshTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(mesh, mesh);
		meshTransform.SetParent(&rootTransform);
		assert(meshInfo.Model && meshInfo.Material);
		g_EnttRegistry.emplace<ecs::CStaticRenderable>(mesh, ecs::CStaticRenderable{ .Vertices = *meshInfo.Model, .Indices = meshInfo.Indices, .Material = *meshInfo.Material });
	}

#if RAD_ENABLE_EXPERIMENTAL
    g_Cam.Position = { -20, 38, -19, 0};
    g_Cam.Rotation = { 0.7, 0.75, 0 };
    Terrain = std::make_unique<TerrainRenderData>();
    Terrain->Terrain = g_TerrainGenerator.InitializeTerrain(g_pd3dDevice.Get(), FrameIndependentCtx, g_pd3dCommandList.Get(), 512, 512, 1024);
	g_TerrainGenerator.GenerateBaseHeightMap(g_pd3dDevice.Get(), FrameIndependentCtx, g_pd3dCommandList.Get(), Terrain->Terrain, ErosionParams);
    auto* terrainRoot = g_SceneTree.AddObject(MeshObject("TerrainRoot"));
    terrainRoot->Scale *= 0.1f;
    terrainRoot->Position = DirectX::XMVectorSet(-13, 15, -10, 0);
    //terrainRoot->Rotation = DirectX::XMVectorSet(-0.5f, 0, 0, 0);
	hlsl::MaterialBuffer terrainMaterial = {};
    terrainMaterial.Diffuse = DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    terrainRoot->Children.emplace_back("Terrain", Terrain->Terrain.TerrainModel->ToModelView(), &*Terrain->Terrain.TerrainMaterial);
    terrainRoot->Children.push_back(MeshObject("Water", Terrain->Terrain.WaterModel->ToModelView(), &*Terrain->Terrain.WaterMaterial));
    g_TextureSelections["TerrainHeightMap"] = []()
        {
            return std::pair{ &Terrain->Terrain.HeightMap, Terrain->Terrain.HeightMap.SRV.GetView() };
        };
	g_TextureSelections["TerrainWaterHeightMap"] = []()
		{
			return std::pair{ &Terrain->Terrain.WaterHeightMap, Terrain->Terrain.WaterHeightMap.SRV.GetView() };
		};
    g_TextureSelections["TerrainWaterOutfluxMap"] = []()
        {
            return std::pair{ &Terrain->Terrain.WaterOutflux, Terrain->Terrain.WaterOutflux.SRV.GetView() };
        };

    g_TextureSelections["TerrainVelocityMap"] = []()
        {
            return std::pair{ &Terrain->Terrain.VelocityMap, Terrain->Terrain.VelocityMap.SRV.GetView() };
        };
    g_TextureSelections["TerrainSedimentMap"] = []()
        {
            return std::pair{ &Terrain->Terrain.SedimentMap, Terrain->Terrain.SedimentMap.SRV.GetView() };
        };
	g_TextureSelections["TerrainNormalMap"] = []()
		{
			return std::pair{ &Terrain->Terrain.TerrainNormalMap, Terrain->Terrain.TerrainNormalMap.SRV.GetView() };
		};
	g_TextureSelections["TextureSoftnessMap"] = []()
		{
			return std::pair{ &Terrain->Terrain.SoftnessMap, Terrain->Terrain.SoftnessMap.SRV.GetView() };
		};
    g_TextureSelections["TerrainThermalPipe1"] = []()
		{
			return std::pair{ &Terrain->Terrain.ThermalPipe1, Terrain->Terrain.ThermalPipe1.SRV.GetView() };
		};
    g_TextureSelections["TerrainThermalPipe2"] = []()
        {
            return std::pair{ &Terrain->Terrain.ThermalPipe2, Terrain->Terrain.ThermalPipe2.SRV.GetView() };
        };
#endif
	auto fence = DXFence::Create(L"SceneLoadFence", g_Renderer.GetDevice());
	g_Renderer.SubmitFrameIndependentCommands(fence, 1, true);
	CloseHandle(fence.FenceEvent);
}

}

// Main code
int main(int argv, char** args)
{
    using namespace::rad;
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

    CreateConsole();
   
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to the latest version of SDL is recommended!)

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    g_SDLWindow = SDL_CreateWindow("DX12 Playground", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_Width, g_Height, window_flags);
    if (g_SDLWindow == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(g_SDLWindow, &wmInfo);
    HWND hwnd = (HWND)wmInfo.info.win.window;
    g_hWnd = hwnd;
    // Initialize the Renderer
	if (!InitRenderer(hwnd, g_Width, g_Height))
    {
		g_Renderer.Deinitialize();
        return 1;
    }

    InitGame();
    LoadSceneData();
    SDL_SetRelativeMouseMode(SDL_TRUE);
    // Main loop
    bool done = false;

    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
	auto& inputMan = InputManager::Get();

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
            g_EnttSystems.UISystem.ProcessEvent(sdlEvent);
            if (sdlEvent.type == SDL_QUIT)
                done = true;
            if (sdlEvent.type == SDL_WINDOWEVENT && sdlEvent.window.event == SDL_WINDOWEVENT_CLOSE && sdlEvent.window.windowID == SDL_GetWindowID(g_SDLWindow))
                done = true;
            if (sdlEvent.type == SDL_WINDOWEVENT && sdlEvent.window.event == SDL_WINDOWEVENT_RESIZED && sdlEvent.window.windowID == SDL_GetWindowID(g_SDLWindow))
            {
                g_Width = sdlEvent.window.data1;
                g_Height = sdlEvent.window.data2;
                // Release all outstanding references to the swap chain's buffers before resizing.
				g_Renderer.OnWindowResized(g_Width, g_Height);
            }
            if (sdlEvent.type == SDL_KEYDOWN)
				inputMan.CUR_KEYS[sdlEvent.key.keysym.scancode] = sdlEvent.key.repeat ? InputManager::KEY_DOWN : InputManager::KEY_PRESSED;
            if (sdlEvent.type == SDL_KEYUP)
				inputMan.CUR_KEYS[sdlEvent.key.keysym.scancode] = InputManager::KEY_RELEASED;
            if (sdlEvent.type == SDL_MOUSEMOTION)
				inputMan.Immediate.MouseDelta = { float(sdlEvent.motion.xrel), float(sdlEvent.motion.yrel) };
            if (sdlEvent.type == SDL_MOUSEWHEEL)
				inputMan.Immediate.MouseWheelDelta = sdlEvent.wheel.y;
        }
		auto now = std::chrono::high_resolution_clock::now();
		auto deltaTime = std::chrono::duration<float>(now - lastTime).count();
		lastTime = now;

		auto frameRec = g_Renderer.BeginFrame();
        UpdateGame(deltaTime, frameRec);

		g_Renderer.EnqueueFrame(std::move(frameRec));
		g_Renderer.RenderPendingFrameRecods();

        for (int i = 0; i < 322; i++)
        {
            if (inputMan.CUR_KEYS[i] == InputManager::KEY_PRESSED)
				inputMan.CUR_KEYS[i] = InputManager::KEY_DOWN;
            if (inputMan.CUR_KEYS[i] == InputManager::KEY_RELEASED)
				inputMan.CUR_KEYS[i] = InputManager::KEY_UP;
        }

        inputMan.Immediate = {};
    }

	g_Renderer.WaitAllCommandContexts();

    // Cleanup
	g_EnttSystems.UISystem.Destroy();

    g_Renderer.Deinitialize();
    SDL_DestroyWindow(g_SDLWindow);
    SDL_Quit();

    return 0;
}