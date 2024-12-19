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

extern "C"
{
	__declspec(dllexport) extern const unsigned int D3D12SDKVersion = DIRECT3D_AGILITY_SDK_VERSION;
}
extern "C"
{
	__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

namespace rad
{

// Data
static SDL_Window* g_SDLWindow = nullptr;
static HWND g_hWnd = nullptr;
static Renderer g_Renderer;
static entt::registry g_EnttRegistry;

struct EnttSystems
{
	EnttSystems(Renderer& renderer) : TerrainErosionSystem(renderer) {}
	ecs::CCameraSystem CameraSystem{};
	ecs::CViewpointControllerSystem ViewpointControllerSystem{};
	ecs::CLightSystem LightSystem{};
	ecs::CStaticRenderSystem StaticRenderSystem{};
	ecs::CUISystem UISystem{};
	proc::TerrainErosionSystem TerrainErosionSystem;
};
static std::unique_ptr<EnttSystems> g_EnttSystems;

static int g_Width = 1920;
static int g_Height = 1080;

void LoadSceneData();

void CreateConsole()
{
	if (!AllocConsole())
	{
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
	HANDLE hConOut = CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	HANDLE hConIn = CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
							   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
	g_EnttSystems = std::make_unique<EnttSystems>(g_Renderer);
	g_EnttSystems->StaticRenderSystem.Init(g_Renderer);
	g_EnttSystems->UISystem.Init(g_Renderer, g_SDLWindow);
	g_EnttSystems->TerrainErosionSystem.Setup();

	auto camera = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(camera, "Camera");
	auto& camSceneTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(camera, camera);
	g_EnttRegistry.emplace<ecs::CCamera>(camera);
	auto& viewpoint =
		g_EnttRegistry.emplace<ecs::CViewpoint>(camera, ecs::CViewpoint{.Projection = ecs::CViewpoint::Perspective{
																			.Fov = 60.0f,
																			.Near = 0.1f,
																			.Far = 1000.0f,
																			.AspectRatio = 16.0f / 9.0f,
																		}});
	ecs::Transform camTransform{};
	camTransform.Position = {5.3f, 2.f, -1.2f};
	camTransform.Rotation = {0.15f, -1.348f, 0.f};
	camSceneTransform.SetTransform(camTransform);
	auto& controller = g_EnttRegistry.emplace<ecs::CViewpointController>(
		camera, ecs::CViewpointController(camSceneTransform.GetWorldTransform(), viewpoint));

	auto dirLight = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(dirLight, "DirectionalLight");
	auto& lightSceneTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(dirLight, dirLight);
	ecs::Transform lightTransform{};
	lightTransform.Position = {10.0f, 24.5f, -3.5f};
	lightTransform.Rotation = {1.0f, -1.2f, 0.f};
	lightSceneTransform.SetTransform(lightTransform);
	g_EnttRegistry.emplace<ecs::CLight>(dirLight);
	auto& lightViewpoint = g_EnttRegistry.emplace<ecs::CViewpoint>(
		dirLight, ecs::CViewpoint{.Projection = ecs::CViewpoint::Orthographic{.Width = 50.0f, .Height = 50.0f}});
	g_EnttRegistry.emplace<ecs::CViewpointController>(
		dirLight, ecs::CViewpointController(lightSceneTransform.GetWorldTransform(), lightViewpoint));
}

void UpdateGame(float deltaTime, RenderFrameRecord& frameRecord)
{
	g_EnttSystems->TerrainErosionSystem.Update(g_EnttRegistry, InputManager::Get(), frameRecord);
	g_EnttSystems->ViewpointControllerSystem.Update(g_EnttRegistry, InputManager::Get(), deltaTime, g_Renderer);
	g_EnttSystems->CameraSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems->LightSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems->StaticRenderSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems->UISystem.Update(g_EnttRegistry, g_Renderer);
}

bool InitRenderer(HWND window, uint32_t width, uint32_t height)
{
	return g_Renderer.Initialize(
#ifdef NDEBUG
		false,
#else
		true,
#endif
		window, width, height);
}

void LoadSceneData()
{
	OptionalRef<ObjModel> sponzaObj{};
	g_Renderer.FrameIndependentCommand(
		[&](CommandContext& commmandCtx)
		{ sponzaObj = g_Renderer.ModelManager->LoadModel(RAD_SPONZA_DIR "sponza.obj", commmandCtx); });
	if (!sponzaObj)
	{
		std::cout << "Failed to load sponza model" << std::endl;
		return;
	}
	entt::entity sponzaRoot = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(sponzaRoot, "SponzaRoot");
	auto& rootTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(sponzaRoot, sponzaRoot);
	rootTransform.SetTransform(ecs::Transform{.Scale = glm::vec3(0.01f)});
	for (auto& [name, meshInfo] : sponzaObj->Meshes)
	{
		entt::entity mesh = g_EnttRegistry.create();
		g_EnttRegistry.emplace<ecs::CEntityInfo>(mesh, name);
		auto& meshTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(mesh, mesh);
		meshTransform.SetParent(&rootTransform);
		assert(meshInfo.Model && meshInfo.Material);
		g_EnttRegistry.emplace<ecs::CStaticRenderable>(mesh, ecs::CStaticRenderable{.Vertices = *meshInfo.Model,
																					.Indices = meshInfo.Indices,
																					.Material = *meshInfo.Material});
	}

	{
		auto& terrainSystem = g_EnttSystems->TerrainErosionSystem;
		entt::entity terrainEnt = g_EnttRegistry.create();
		g_EnttRegistry.emplace<ecs::CEntityInfo>(terrainEnt, "Terrain");
		auto& terrainTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(terrainEnt, terrainEnt);
		auto& terrain = g_EnttRegistry.emplace<proc::CTerrain>(terrainEnt, terrainSystem.CreateTerrain(1024));
		CommandRecord cmdRec{};
		auto& indexedPlane =
			g_EnttRegistry.emplace<proc::CIndexedPlane>(terrainEnt, terrainSystem.CreatePlane(cmdRec, 512, 512));
		auto& erosionParams = g_EnttRegistry.emplace<proc::CErosionParameters>(terrainEnt, proc::CErosionParameters{});
		auto& terrainRenderable = g_EnttRegistry.emplace<proc::CTerrainRenderable>(
			terrainEnt, terrainSystem.CreateTerrainRenderable(terrain));
		auto& waterRenderable =
			g_EnttRegistry.emplace<proc::CWaterRenderable>(terrainEnt, terrainSystem.CreateWaterRenderable(terrain));

		terrainSystem.GenerateBaseHeightMap(cmdRec, terrain, erosionParams, terrainRenderable, waterRenderable);

		g_Renderer.FrameIndependentCommand(
			[cmdRec = std::move(cmdRec)](CommandContext& commandCtx) mutable
			{
				while (!cmdRec.Queue.empty())
				{
					auto& [name, cmd] = cmdRec.Queue.front();
					cmd(commandCtx);
					cmdRec.Queue.pop();
				}
			});

		ecs::Transform transform{};
		transform.Scale = {50.0f, 0.05f, 50.0f};
		transform.Position = glm::vec3(-5, 15, 0);
		terrainTransform.SetTransform(transform);
		// terrainRoot->Rotation = DirectX::XMVectorSet(-0.5f, 0, 0, 0);
		hlsl::MaterialBuffer terrainMaterial = {};
		g_Renderer.ViewableTextures.emplace(
			"TerrainHeightMap",
			std::pair<Ref<DXTexture>, DescriptorAllocationView>{*terrain.HeightMap, terrain.HeightMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainWaterHeightMap",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.WaterHeightMap, terrain.WaterHeightMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainWaterOutfluxMap",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.WaterOutflux, terrain.WaterOutflux->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainVelocityMap",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.VelocityMap, terrain.VelocityMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainSedimentMap",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.SedimentMap, terrain.SedimentMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TextureSoftnessMap",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.SoftnessMap, terrain.SoftnessMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainThermalPipe1",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.ThermalPipe1, terrain.ThermalPipe1->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainThermalPipe2",
											std::pair<Ref<DXTexture>, DescriptorAllocationView>{
												*terrain.ThermalPipe2, terrain.ThermalPipe2->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainAlbedoMap", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
																	*terrainRenderable.TerrainAlbedoTex,
																	terrainRenderable.TerrainAlbedoTex->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace("TerrainNormalMap", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
																	*terrainRenderable.TerrainNormalMap,
																	terrainRenderable.TerrainNormalMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace(
			"WaterAlbedoMap", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
								  *waterRenderable.WaterAlbedoMap, waterRenderable.WaterAlbedoMap->SRV.GetView()});
		g_Renderer.ViewableTextures.emplace(
			"WaterNormalMap", std::pair<Ref<DXTexture>, DescriptorAllocationView>{
								  *waterRenderable.WaterNormalMap, waterRenderable.WaterNormalMap->SRV.GetView()});
	}
	auto fence = DXFence::Create(L"SceneLoadFence", g_Renderer.GetDevice());
	g_Renderer.SubmitFrameIndependentCommands(fence, 1, true);
	CloseHandle(fence.FenceEvent);
}

} // namespace rad

// Main code
int main(int argv, char** args)
{
	using namespace ::rad;
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
	// (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows
	// systems, depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to the latest version of
	// SDL is recommended!)

	// Setup window
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	g_SDLWindow = SDL_CreateWindow("DX12 Playground", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_Width, g_Height,
								   window_flags);
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
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
		// inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or
		// clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
		// clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and
		// hide them from your application based on those two flags.
		SDL_Event sdlEvent;
		while (SDL_PollEvent(&sdlEvent))
		{
			g_EnttSystems->UISystem.ProcessEvent(sdlEvent);
			if (sdlEvent.type == SDL_QUIT)
				done = true;
			if (sdlEvent.type == SDL_WINDOWEVENT && sdlEvent.window.event == SDL_WINDOWEVENT_CLOSE &&
				sdlEvent.window.windowID == SDL_GetWindowID(g_SDLWindow))
				done = true;
			if (sdlEvent.type == SDL_WINDOWEVENT && sdlEvent.window.event == SDL_WINDOWEVENT_RESIZED &&
				sdlEvent.window.windowID == SDL_GetWindowID(g_SDLWindow))
			{
				g_Width = sdlEvent.window.data1;
				g_Height = sdlEvent.window.data2;
				// Release all outstanding references to the swap chain's buffers before resizing.
				g_Renderer.OnWindowResized(g_Width, g_Height);
			}
			if (sdlEvent.type == SDL_KEYDOWN)
				inputMan.CUR_KEYS[sdlEvent.key.keysym.scancode] =
					sdlEvent.key.repeat ? InputManager::KEY_DOWN : InputManager::KEY_PRESSED;
			if (sdlEvent.type == SDL_KEYUP)
				inputMan.CUR_KEYS[sdlEvent.key.keysym.scancode] = InputManager::KEY_RELEASED;
			if (sdlEvent.type == SDL_MOUSEMOTION)
				inputMan.Immediate.MouseDelta = {float(sdlEvent.motion.xrel), float(sdlEvent.motion.yrel)};
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
	g_EnttSystems->UISystem.Destroy();

	g_Renderer.Deinitialize();
	SDL_DestroyWindow(g_SDLWindow);
	SDL_Quit();

	return 0;
}