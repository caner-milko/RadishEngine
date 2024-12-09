#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_dx12.h"
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
static HWND g_hWnd = nullptr;
static Renderer g_Renderer;
static entt::registry g_EnttRegistry;
static entt::entity g_Camera;
static entt::entity g_DirectionalLight;

static struct EnttSystems
{
	ecs::CCameraSystem CameraSystem{};
	ecs::CLightSystem LightSystem{};
	ecs::CStaticRenderSystem StaticRenderSystem{};
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
        glm::vec2 MouseDelta = { 0, 0 };
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

//void UIDrawMeshTree(MeshObject* object)
//{
//    ImGui::PushID(object->Name.c_str());
//
//	if (ImGui::TreeNodeEx(object->Name.c_str(), ImGuiTreeNodeFlags_Framed))
//    {
//		ImGui::InputFloat3("Position", &object->Position.m128_f32[0], "%.3f");
//		ImGui::InputFloat3("Rotation", &object->Rotation.m128_f32[0], "%.3f");
//		ImGui::InputFloat3("Scale", &object->Scale.m128_f32[0], "%.3f");
//
//        ImGui::Checkbox("TransformOnly", &object->TransformOnly);
//	    for (auto& child : object->Children)
//        {
//		    UIDrawMeshTree(&child);
//	    }
//		ImGui::TreePop();
//	}   
//
//    ImGui::PopID();
//}

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

    {
        ImGui::Begin("Scene");
        //Camera
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
			ImGui::PushID("Camera");
			auto& sceneTransform = g_EnttRegistry.get<ecs::CSceneTransform>(g_Camera);
			auto transform = sceneTransform.LocalTransform();
            ImGui::InputFloat3("Position", &transform.Position.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat3("Rotation", &transform.Rotation.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
			auto dir = sceneTransform.GetWorldTransform().GetForward();
			ImGui::InputFloat3("Direction", &dir.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
			auto& viewpoint = g_EnttRegistry.get<ecs::CViewpoint>(g_Camera);
			if(auto* perspective = std::get_if<ecs::CViewpoint::Perspective>(&viewpoint.Projection))
				ImGui::InputFloat("FoV", &perspective->Fov, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_ReadOnly);
			if (auto* cameraController = g_EnttRegistry.try_get<ecs::CCameraController>(g_Camera))
			{
				ImGui::SliderFloat("Move Speed", &cameraController->MoveSpeed, 0.0f, 10000.0f);
				ImGui::SliderFloat("Rotation Speed", &cameraController->RotateSpeed, 0.1f, 10.0f);
				if (ImGui::Button("Reset"))
					viewpoint = cameraController->OriginalViewpoint;
			}
			ImGui::PopID();
        }

		//Light
		if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID("Light");
			auto& sceneTransform = g_EnttRegistry.get<ecs::CSceneTransform>(g_DirectionalLight);
			auto transform = sceneTransform.LocalTransform();
			bool transformChanged = ImGui::InputFloat3("Position", &transform.Position.x, "%.3f");
			transformChanged |= ImGui::InputFloat3("Rotation", &transform.Rotation.x, "%.3f");
            auto dir = sceneTransform.GetWorldTransform().GetForward();
			ImGui::InputFloat3("Direction", &dir.x, "%.3f", ImGuiSliderFlags_NoInput);

			auto& light = g_EnttRegistry.get<ecs::CLight>(g_DirectionalLight);
            ImGui::ColorEdit3("Color", &light.Color.x);
			ImGui::SliderFloat("Intensity", &light.Intensity, 0.0f, 10.0f);
			ImGui::ColorEdit3("Ambient Color", &light.Ambient.x);


#if RAD_ENABLE_EXPERIMENTAL
            float yawDegrees = XMConvertToDegrees(g_DirectionalLight.Rotation.m128_f32[0]);
            ImGui::SliderFloat("Yaw", &yawDegrees, 0.0f, 360.0f, "%.3f", ImGuiSliderFlags_NoInput);
			g_DirectionalLight.Rotation.m128_f32[0] = XMConvertToRadians(yawDegrees);
            float pitchDegress = XMConvertToDegrees(g_DirectionalLight.Rotation.m128_f32[1]);
			ImGui::SliderFloat("Pitch", &pitchDegress, -90.0f, 90.0f, "%.3f", ImGuiSliderFlags_NoInput);
			g_DirectionalLight.Rotation.m128_f32[1] = XMConvertToRadians(pitchDegress);
			if (ImGui::Button("Reset"))
				g_DirectionalLight.Reset();
#endif
			ImGui::PopID();
        }

#if RAD_ENABLE_EXPERIMENTAL
		if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID("Terrain");
			if (ImGui::Button("Generate Base Height Map"))
				g_RenderingTasks.push([]()
					{
						g_TerrainGenerator.GenerateBaseHeightMap(g_pd3dDevice.Get(), FrameIndependentCtx, g_pd3dCommandList.Get(), Terrain->Terrain, ErosionParams);
					});
			if (ImGui::Button("Erode Terrain"))
				g_RenderingTasks.push([]()
					{
						g_TerrainGenerator.ErodeTerrain(g_pd3dDevice.Get(), FrameIndependentCtx, g_pd3dCommandList.Get(), Terrain->Terrain, ErosionParams);
					});
			ImGui::Checkbox("With Water", &ErosionParams.MeshWithWater);
			ImGui::Checkbox("Base from File", &ErosionParams.BaseFromFile);
            if (!ErosionParams.BaseFromFile)
            {
                ImGui::SliderFloat("Initial Roughness", &ErosionParams.InitialRoughness, 0.0f, 2.0f);
			    ImGui::Checkbox("Random", &ErosionParams.Random);
			    if (!ErosionParams.Random)
			    	ImGui::SliderInt("Seed", &ErosionParams.Seed, 0, 100000);
            }
			ImGui::SliderFloat("Min Height", &ErosionParams.MinHeight, 0.0f, 100.0f);
			ImGui::SliderFloat("Max Height", &ErosionParams.MaxHeight, 0.0f, 200.0f);
			ImGui::Checkbox("Erode Each Frame", &ErosionParams.ErodeEachFrame);
            ImGui::SliderInt("Iterations", &ErosionParams.Iterations, 1, 1024);
			ImGui::SliderFloat("Total Length", &ErosionParams.TotalLength, 100.0f, 2048.0f);


			ImGui::SliderFloat("Rain Rate", &ErosionParams.RainRate, 0.0f, 0.1f);
			ImGui::SliderFloat("Pipe Cross Section", &ErosionParams.PipeCrossSection, 0.0f, 100.0f);
			ImGui::SliderFloat("Evaporation Rate", &ErosionParams.EvaporationRate, 0.0f, 0.1f);
			ImGui::SliderFloat("Sediment Capacity", &ErosionParams.SedimentCapacity, 0.0f, 2.0f);
			ImGui::SliderFloat("Soil Suspension Rate", &ErosionParams.SoilSuspensionRate, 0.0f, 2.f);
			ImGui::SliderFloat("Sediment Deposition Rate", &ErosionParams.SedimentDepositionRate, 0.0f, 3.0f);
            ImGui::SliderFloat("Soil Hardening Rate", &ErosionParams.SoilHardeningRate, 0.0f, 2.0f);
            ImGui::SliderFloat("Soil Softening Rate", &ErosionParams.SoilSofteningRate, 0.0f, 2.0f);
            ImGui::SliderFloat("Minimum Soil Softness", &ErosionParams.MinimumSoilSoftness, 0.0f, 1.0f);
			ImGui::SliderFloat("Maximal Erosion Depth", &ErosionParams.MaximalErosionDepth, 0.0f, 40.0f);
			
            ImGui::SliderFloat("Softness Talus Coefficient", &ErosionParams.SoftnessTalusCoefficient, 0.0f, 1.0f);
			ImGui::SliderFloat("Min Talus Coefficient", &ErosionParams.MinTalusCoefficient, 0.0f, 1.0f);
			ImGui::SliderFloat("Thermal Erosion Rate", &ErosionParams.ThermalErosionRate, 0.0f, 5.0f);
			
            ImGui::PopID();
		}

        if (ImGui::CollapsingHeader("Texture View"))
        {
			if (ImGui::BeginCombo("Textures", g_SelectedTexture.c_str()))
			{
				for (auto& [name, func] : g_TextureSelections)
				{
					if (ImGui::Selectable(name.c_str()))
						g_SelectedTexture = name;
				}
                if (ImGui::Selectable("None"))
                {
					g_SelectedTexture = "None";
                }
				ImGui::EndCombo();
			}
        }

		UIDrawMeshTree(&g_SceneTree.Root);
#endif
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
}

void InitGame()
{
    memset(g_IO.CUR_KEYS, 0, sizeof(g_IO.CUR_KEYS));

	g_EnttSystems = {};
	g_EnttSystems.StaticRenderSystem.Init(g_Renderer);

	g_Camera = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(g_Camera, "Camera");
	g_EnttRegistry.emplace<ecs::CSceneTransform>(g_Camera);
	g_EnttRegistry.emplace<ecs::CCamera>(g_Camera);
	auto& viewpoint =  g_EnttRegistry.emplace<ecs::CViewpoint>(g_Camera, ecs::CViewpoint{ .Projection = ecs::CViewpoint::Perspective{.Fov = glm::radians(60.0f), 
		.Near = 0.1f, .Far = 1000.0f, .AspectRatio = 16.0f / 9.0f, }});
	auto& controller = g_EnttRegistry.emplace<ecs::CCameraController>(g_Camera, ecs::CCameraController(viewpoint));

	g_DirectionalLight = g_EnttRegistry.create();
	g_EnttRegistry.emplace<ecs::CEntityInfo>(g_DirectionalLight, "DirectionalLight");
	g_EnttRegistry.emplace<ecs::CSceneTransform>(g_DirectionalLight);
	g_EnttRegistry.emplace<ecs::CLight>(g_DirectionalLight);
	g_EnttRegistry.emplace<ecs::CViewpoint>(g_DirectionalLight, ecs::CViewpoint{ .Projection = ecs::CViewpoint::Orthographic{.Width = 100.0f, .Height = 100.0f} });
}

void UpdateGame(float deltaTime, RenderFrameRecord& frameRecord)
{
    if (g_IO.IsKeyPressed(SDL_SCANCODE_ESCAPE))
	{
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	}
    if (g_IO.IsKeyPressed(SDL_SCANCODE_E))
    {
		//Disable imgui navigation
		ImGui::GetIO().ConfigFlags ^= ImGuiConfigFlags_NavEnableKeyboard;
        SDL_SetRelativeMouseMode((SDL_bool)g_IO.CursorEnabled);
        g_IO.CursorEnabled = !g_IO.CursorEnabled;
    }
    if (g_IO.CursorEnabled)
        return;
    // Switch controlled object on Tab
#if RAD_ENABLE_EXPERIMENTAL
	if (g_IO.IsKeyPressed(SDL_SCANCODE_TAB))
	{
        if (g_Controlled == &g_Cam)
            g_Controlled = &g_DirectionalLight;
        else
            g_Controlled = &g_Cam;
	}

	if (g_IO.IsKeyPressed(SDL_SCANCODE_L))
    {
		g_DirectionalLight.Position = g_Cam.Position;
		g_DirectionalLight.Rotation = g_Cam.Rotation;
    }
    
    auto& controlled = *g_Controlled;
    if (g_IO.IsKeyPressed(SDL_SCANCODE_R))
        controlled.Reset();
    if (g_Controlled == &g_Cam)
    {
		auto& cam = static_cast<Camera&>(*g_Controlled);
		cam.SetFoV(cam.FoV - g_IO.Immediate.MouseWheelDelta * 2.0f);
    }
    else
    {
		g_DirectionalLight.SetInverseZoom(g_DirectionalLight.InverseZoom - g_IO.Immediate.MouseWheelDelta * 2.0f);
    }
    Vector4 moveDir = { float(g_IO.IsKeyDown(SDL_SCANCODE_D)) - float(g_IO.IsKeyDown(SDL_SCANCODE_A)), 0
        , float(g_IO.IsKeyDown(SDL_SCANCODE_W)) - float(g_IO.IsKeyDown(SDL_SCANCODE_S)), 0 };
        
	Vector4 moveVec = XMVector4Transform(XMVector4Normalize(moveDir), controlled.GetRotationMatrix());

    moveVec.m128_f32[1] += float(g_IO.IsKeyDown(SDL_SCANCODE_SPACE)) - float(g_IO.IsKeyDown(SDL_SCANCODE_LCTRL));
    
    moveVec = XMVector4Normalize(moveVec);

    controlled.Position = controlled.Position + moveVec * deltaTime * controlled.MoveSpeed;
        
    if(!g_IO.CursorEnabled)
    {
        controlled.Rotation = controlled.Rotation + XMVectorSet(g_IO.Immediate.MouseDelta.y, g_IO.Immediate.MouseDelta.x, 0, 0) * controlled.RotSpeed * deltaTime;
        controlled.Rotation = XMVectorSetX(controlled.Rotation, std::clamp(XMVectorGetX(controlled.Rotation), -XM_PIDIV2 + 0.0001f, XM_PIDIV2 - 0.0001f));
    }
#endif 
	
	g_EnttSystems.CameraSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems.LightSystem.Update(g_EnttRegistry, frameRecord);
	g_EnttSystems.StaticRenderSystem.Update(g_EnttRegistry, frameRecord);

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
	auto& rootTransform = g_EnttRegistry.emplace<ecs::CSceneTransform>(sponzaRoot);
	rootTransform.SetTransform(ecs::Transform{ .Scale = glm::vec3(0.01f) });
	for (auto& [name, meshInfo] : sponzaObj->Meshes)
	{
		entt::entity mesh = g_EnttRegistry.create();
		g_EnttRegistry.emplace<ecs::CEntityInfo>(mesh, name);
		g_EnttRegistry.emplace<ecs::CSceneTransform>(mesh);
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
    // Initialize the Renderer
	if (!InitRenderer(hwnd, g_Width, g_Height))
    {
		g_Renderer.Deinitialize();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
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
    auto fontAllocation = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
    ImGui_ImplDX12_Init(&g_Renderer.GetDevice(), g_Renderer.FramesInFlight,
        DXGI_FORMAT_R8G8B8A8_UNORM, fontAllocation.Heap->Heap.Get(),
        fontAllocation.GetCPUHandle(),
        fontAllocation.GetGPUHandle());
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
                // Release all outstanding references to the swap chain's buffers before resizing.
				g_Renderer.OnWindowResized(g_Width, g_Height);
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
		auto frameRec = g_Renderer.BeginFrame();
        UpdateGame(io.DeltaTime, frameRec);

		g_Renderer.EnqueueFrame(std::move(frameRec));
		g_Renderer.RenderPendingFrameRecods();

        for (int i = 0; i < 322; i++)
        {
            if (g_IO.CUR_KEYS[i] == IO::KEY_PRESSED)
                g_IO.CUR_KEYS[i] = IO::KEY_DOWN;
            if (g_IO.CUR_KEYS[i] == IO::KEY_RELEASED)
                g_IO.CUR_KEYS[i] = IO::KEY_UP;
        }

        g_IO.Immediate = {};
    }

	g_Renderer.WaitAllCommandContexts();

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    g_Renderer.Deinitialize();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}