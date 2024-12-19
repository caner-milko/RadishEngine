#include "Systems.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"
#include "ProcGen/TerrainGenerator.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_dx12.h"

namespace rad::ecs
{
glm::mat4 Transform::GetModelMatrix(glm::mat4 parentWorldMatrix) const
{
	parentWorldMatrix = glm::translate(parentWorldMatrix, Position);
	parentWorldMatrix = parentWorldMatrix * glm::toMat4(glm::quat(Rotation));
	parentWorldMatrix = glm::scale(parentWorldMatrix, Scale);
	return parentWorldMatrix;
}
WorldTransform::operator ecs::Transform() const
{
	ecs::Transform transform;
	transform.Position = GetPosition();
	transform.Scale = GetScale();
	transform.Rotation = glm::eulerAngles(glm::quat_cast(glm::mat3(WorldMatrix)));
	return transform;
}

bool CStaticRenderSystem::Init(Renderer& renderer)
{
	// Static Mesh Pipeline
	{
		struct StaticMeshPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} pipelineStateStream;

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		auto [vertexShader, pixelShader] = renderer.ShaderManager->CompileBindlessGraphicsShader(
			L"Triangle", RAD_SHADERS_DIR L"Graphics/StaticMesh.hlsl");

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 2;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvFormats.RTFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		pipelineStateStream.RTVFormats = rtvFormats;

		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		StaticMeshPipelineState = PipelineState::Create("StaticMeshPipeline", renderer.GetDevice(), pipelineStateStream,
														&renderer.ShaderManager->BindlessRootSignature);
	}
	// Shadow Map Pipeline
	{
		struct ShadowMapPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		} pipelineStateStream;

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(
			renderer.ShaderManager
				->CompileBindlessVertexShader(L"ShadowMap", RAD_SHADERS_DIR L"Graphics/Shadowmap.hlsl")
				->Blob.Get());

		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		ShadowMapPipelineState = PipelineState::Create("ShadowMapPipeline", renderer.GetDevice(), pipelineStateStream,
													   &renderer.ShaderManager->BindlessRootSignature);
	}

	return true;
}

void CStaticRenderSystem::Update(entt::registry& registry, RenderFrameRecord& frameRecord)
{
	std::vector<StaticRenderData> renderObjects;

	auto view = registry.view<CStaticRenderable, CSceneTransform>();
	for (auto entity : view)
	{
		auto& transform = view.get<CSceneTransform>(entity);
		auto locTransform = transform.LocalTransform();
		transform.SetTransform(locTransform);
		auto& renderable = view.get<CStaticRenderable>(entity);

		StaticRenderData renderData;
		renderData.WorldMatrix = transform.GetWorldTransform().WorldMatrix;
		renderData.IndexCount = renderable.Indices.Size / sizeof(uint32_t);
		renderData.VertexBufferView = renderable.Vertices.VertexBufferView();
		renderData.IndexBufferView = renderable.Indices.IndexBufferView();
		renderData.Material = renderable.Material.MaterialInfo.GetView();

		renderObjects.push_back(renderData);
	}

	frameRecord.Push(TypedRenderCommand<StaticRenderData>{
		.Name = "StaticRender",
		.Data = std::move(renderObjects),
		.DepthOnlyPass = [this](auto span, auto view, auto passData) { DepthOnlyPass(span, view, passData); },
		.DeferredPass = [this](auto span, auto view, auto passData) { DeferredPass(span, view, passData); }});
}
void CStaticRenderSystem::DepthOnlyPass(std::span<StaticRenderData> renderObjects, const RenderView& view,
										DepthOnlyPassData& passData)
{
	auto& cmd = passData.CmdContext;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ShadowMapPipelineState.Bind(cmd);

	StaticRenderData lastRenderData{};
	for (auto& renderObj : renderObjects)
	{
		if (renderObj.VertexBufferView.BufferLocation != lastRenderData.VertexBufferView.BufferLocation)
		{
			lastRenderData.VertexBufferView = renderObj.VertexBufferView;
			cmd->IASetVertexBuffers(0, 1, &renderObj.VertexBufferView);
		}

		if (renderObj.IndexBufferView.BufferLocation != lastRenderData.IndexBufferView.BufferLocation)
		{
			lastRenderData.IndexBufferView = renderObj.IndexBufferView;
			cmd->IASetIndexBuffer(&renderObj.IndexBufferView);
		}
		rad::hlsl::ShadowMapResources shadowMapResources{};
		shadowMapResources.MVP = view.ViewProjectionMatrix * renderObj.WorldMatrix;
		ShadowMapPipelineState.SetResources(cmd, shadowMapResources);
		cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
	}
}
void CStaticRenderSystem::DeferredPass(std::span<StaticRenderData> renderObjects, const RenderView& view,
									   DeferredPassData& passData)
{
	auto& cmd = passData.CmdContext;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	StaticMeshPipelineState.Bind(cmd);

	StaticRenderData lastRenderData{};
	for (auto& renderObj : renderObjects)
	{
		if (renderObj.VertexBufferView.BufferLocation != lastRenderData.VertexBufferView.BufferLocation)
		{
			lastRenderData.VertexBufferView = renderObj.VertexBufferView;
			cmd->IASetVertexBuffers(0, 1, &renderObj.VertexBufferView);
		}

		if (renderObj.IndexBufferView.BufferLocation != lastRenderData.IndexBufferView.BufferLocation)
		{
			lastRenderData.IndexBufferView = renderObj.IndexBufferView;
			cmd->IASetIndexBuffer(&renderObj.IndexBufferView);
		}
		rad::hlsl::StaticMeshResources staticMeshResources{};
		staticMeshResources.MVP = view.ViewProjectionMatrix * renderObj.WorldMatrix;
		staticMeshResources.Normal = glm::transpose(glm::inverse(renderObj.WorldMatrix));
		staticMeshResources.MaterialBufferIndex = renderObj.Material.GetIndex();
		cmd->SetGraphicsRoot32BitConstants(0, sizeof(staticMeshResources) / 4, &staticMeshResources, 0);
		cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
	}
}

glm::mat4 CViewpoint::ViewMatrix(CSceneTransform const& sceneTransform) const
{
	return glm::inverse(sceneTransform.GetWorldTransform().WorldMatrix);
}
glm::mat4 CViewpoint::ProjectionMatrix() const
{
	if (auto* perspective = std::get_if<Perspective>(&Projection))
	{
		return glm::perspectiveLH(glm::radians(perspective->Fov), perspective->AspectRatio, perspective->Near,
								  perspective->Far);
	}
	else
	{
		auto orthographic = std::get<Orthographic>(Projection);
		return glm::orthoLH(-orthographic.Width / 2.0f, orthographic.Width / 2.0f, -orthographic.Height / 2.0f,
							orthographic.Height / 2.0f, 0.1f, 100.0f);
	}
}
RenderView ViewpointToRenderView(const CViewpoint& viewpoint, const CSceneTransform& transform)
{
	RenderView view;
	view.ViewMatrix = viewpoint.ViewMatrix(transform);
	view.ProjectionMatrix = viewpoint.ProjectionMatrix();
	view.ViewPosition = transform.GetWorldTransform().GetPosition();
	view.ViewDirection = transform.GetWorldTransform().GetForward();
	view.ViewProjectionMatrix = view.ProjectionMatrix * view.ViewMatrix;
	return view;
}
void CCameraSystem::Update(entt::registry& registry, RenderFrameRecord& frameRecord)
{
	auto view = registry.view<CCamera, CViewpoint, CSceneTransform>();
	for (auto entity : view)
	{
		auto& viewpoint = view.get<CViewpoint>(entity);
		auto& transform = view.get<CSceneTransform>(entity);
		auto viewMatrix = viewpoint.ViewMatrix(transform);
		auto projectionMatrix = viewpoint.ProjectionMatrix();
		auto worldTransform = transform.GetWorldTransform();

		frameRecord.View = ViewpointToRenderView(viewpoint, transform);
	}
}
void CLightSystem::Update(entt::registry& registry, RenderFrameRecord& frameRecord)
{
	auto view = registry.view<CLight, CViewpoint, CSceneTransform>();
	for (auto entity : view)
	{
		auto& viewpoint = view.get<CViewpoint>(entity);
		auto& light = view.get<CLight>(entity);
		auto& transform = view.get<CSceneTransform>(entity);
		auto viewMatrix = viewpoint.ViewMatrix(transform);
		auto projectionMatrix = viewpoint.ProjectionMatrix();
		auto worldTransform = transform.GetWorldTransform();

		frameRecord.LightInfo = {
			.View = ViewpointToRenderView(viewpoint, transform), .Color = light.Color, .Intensity = light.Intensity};
	}
}
void CViewpointControllerSystem::Update(entt::registry& registry, InputManager& io, float deltaTime, Renderer& renderer)
{

	if (io.IsKeyPressed(SDL_SCANCODE_ESCAPE))
	{
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	}
	if (io.IsKeyPressed(SDL_SCANCODE_E))
	{
		// Disable imgui navigation
		ImGui::GetIO().ConfigFlags ^= ImGuiConfigFlags_NavEnableKeyboard;
		SDL_SetRelativeMouseMode((SDL_bool)io.CursorEnabled);
		io.CursorEnabled = !io.CursorEnabled;
	}
	if (InputManager::Get().CursorEnabled)
		return;

	auto cameraEntity = registry.view<CCamera, CViewpointController, CSceneTransform, CViewpoint>().front();

	auto& camViewpoint = registry.get<CViewpoint>(cameraEntity);
	auto& camController = registry.get<CViewpointController>(cameraEntity);
	auto& camTransform = registry.get<CSceneTransform>(cameraEntity);

	auto lightEntity = registry.view<CLight, CSceneTransform, CViewpointController, CViewpoint>().front();
	auto& lightTransform = registry.get<CSceneTransform>(lightEntity);
	auto& lightViewpoint = registry.get<CViewpoint>(lightEntity);
	auto& lightController = registry.get<CViewpointController>(lightEntity);

	if (io.IsKeyPressed(SDL_SCANCODE_L))
		lightTransform.SetTransform(camTransform.GetWorldTransform());

	if (ActiveViewpoint == entt::null)
		ActiveViewpoint = cameraEntity;

	if (io.IsKeyPressed(SDL_SCANCODE_TAB))
	{
		ActiveViewpoint = ActiveViewpoint == cameraEntity ? lightEntity : cameraEntity;
		if (!renderer.ViewingTexture || renderer.ViewingTexture == "ShadowMap")
			renderer.ViewingTexture =
				ActiveViewpoint == cameraEntity ? std::nullopt : std::optional<std::string>("ShadowMap");
	}

	auto& controlledViewpoint = registry.get<CViewpoint>(ActiveViewpoint);
	auto& controlledController = registry.get<CViewpointController>(ActiveViewpoint);
	auto& controlledTransform = registry.get<CSceneTransform>(ActiveViewpoint);
	if (io.IsKeyPressed(SDL_SCANCODE_R))
	{
		controlledViewpoint = controlledController.OriginalViewpoint;
		controlledTransform.SetTransform(controlledController.OriginalTransform);
	}
	if (auto* perspective = std::get_if<CViewpoint::Perspective>(&controlledViewpoint.Projection))
	{
		perspective->Fov = glm::clamp(perspective->Fov - io.Immediate.MouseWheelDelta * 2.0f, 45.0f, 120.0f);
	}
	else if (auto* orthographic = std::get_if<CViewpoint::Orthographic>(&controlledViewpoint.Projection))
	{
		orthographic->Width = glm::clamp(orthographic->Width - io.Immediate.MouseWheelDelta * 2.0f, 0.1f, 100.0f);
		orthographic->Height = glm::clamp(orthographic->Height - io.Immediate.MouseWheelDelta * 2.0f, 0.1f, 100.0f);
	}

	glm::vec3 moveDir = {float(io.IsKeyDown(SDL_SCANCODE_D)) - float(io.IsKeyDown(SDL_SCANCODE_A)), 0,
						 float(io.IsKeyDown(SDL_SCANCODE_W)) - float(io.IsKeyDown(SDL_SCANCODE_S))};

	if (glm::length(moveDir) > 0.0f)
		moveDir = glm::normalize(moveDir);

	glm::vec3 moveVec = controlledTransform.GetWorldTransform().GetRotation() * moveDir;

	moveVec.y += float(io.IsKeyDown(SDL_SCANCODE_SPACE)) - float(io.IsKeyDown(SDL_SCANCODE_LCTRL));

	if (glm::length(moveVec) > 0.0f)
		moveVec = glm::normalize(moveVec);

	auto curTransform = controlledTransform.LocalTransform();
	curTransform.Position = curTransform.Position + moveVec * deltaTime * controlledController.MoveSpeed;

	if (!io.CursorEnabled)
	{
		curTransform.Rotation =
			curTransform.Rotation + glm::vec3(io.Immediate.MouseDelta.y, io.Immediate.MouseDelta.x, 0) *
										controlledController.RotateSpeed * deltaTime;
		curTransform.Rotation.x =
			glm::clamp(curTransform.Rotation.x, -glm::pi<float>() * .5f + 0.0001f, glm::pi<float>() * .5f - 0.0001f);
	}
	controlledTransform.SetTransform(curTransform);
}

void CUISystem::Init(Renderer& renderer, SDL_Window* window)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForD3D(window);
	auto fontAllocation = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	ImGui_ImplDX12_Init(&renderer.GetDevice(), renderer.FramesInFlight, DXGI_FORMAT_R8G8B8A8_UNORM,
						fontAllocation.Heap->Heap.Get(), fontAllocation.GetCPUHandle(), fontAllocation.GetGPUHandle());
	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
	// ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your
	// application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
	// ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
	// backslash \\ !
	// io.Fonts->AddFontDefault();
	// io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr,
	// io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != nullptr);
}
void CUISystem::Destroy()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}
void CUISystem::ProcessEvent(const SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);
}
static void UIDrawMeshTree(entt::registry& registry, entt::entity curEntity)
{
	std::string name = "Unknown";
	if (auto* entityName = registry.try_get<CEntityInfo>(curEntity))
		name = entityName->Name;
	ImGui::PushID(name.c_str());

	if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Framed))
	{
		auto* sceneTransform = registry.try_get<CSceneTransform>(curEntity);
		if (sceneTransform)
		{
			auto transform = sceneTransform->LocalTransform();
			ImGui::InputFloat3("Local Position", &transform.Position.x, "%.3f");
			ImGui::InputFloat3("Local Rotation", &transform.Rotation.x, "%.3f");
			ImGui::InputFloat3("Local Scale", &transform.Scale.x, "%.3f");
			sceneTransform->SetTransform(transform);
		}
		if (auto* staticRenderable = registry.try_get<CStaticRenderable>(curEntity))
		{
			ImGui::Checkbox("Hidden", &staticRenderable->Hidden);
		}
		if (sceneTransform)
			for (auto& child : sceneTransform->Children)
				UIDrawMeshTree(registry, child->Entity);
		ImGui::TreePop();
	}

	ImGui::PopID();
}
void CUISystem::Update(entt::registry& registry, Renderer& renderer)
{
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplSDL2_NewFrame();

	// Draw UI
	ImGui::NewFrame();

	{
		ImGui::Begin("Scene");
		// Camera
		auto camera = registry.view<CCamera, CViewpoint, CSceneTransform>().front();
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID("Camera");
			auto& sceneTransform = registry.get<ecs::CSceneTransform>(camera);
			auto transform = sceneTransform.LocalTransform();
			ImGui::InputFloat3("Position", &transform.Position.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation", &transform.Rotation.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
			auto dir = sceneTransform.GetWorldTransform().GetForward();
			ImGui::InputFloat3("Direction", &dir.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
			auto& viewpoint = registry.get<ecs::CViewpoint>(camera);
			if (auto* perspective = std::get_if<ecs::CViewpoint::Perspective>(&viewpoint.Projection))
				ImGui::InputFloat("FoV", &perspective->Fov, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_ReadOnly);
			if (auto* cameraController = registry.try_get<ecs::CViewpointController>(camera))
			{
				ImGui::SliderFloat("Move Speed", &cameraController->MoveSpeed, 0.0f, 10000.0f);
				ImGui::SliderFloat("Rotation Speed", &cameraController->RotateSpeed, 0.1f, 10.0f);
				if (ImGui::Button("Reset"))
					viewpoint = cameraController->OriginalViewpoint;
			}
			ImGui::PopID();
		}

		// Light
		auto dirLight = registry.view<CLight, CViewpoint, CSceneTransform, CViewpointController>().front();
		if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID("Light");
			auto& sceneTransform = registry.get<ecs::CSceneTransform>(dirLight);
			auto transform = sceneTransform.LocalTransform();
			bool transformChanged = ImGui::InputFloat3("Position", &transform.Position.x, "%.3f");
			transformChanged |= ImGui::InputFloat3("Rotation", &transform.Rotation.x, "%.3f");
			auto dir = sceneTransform.GetWorldTransform().GetForward();
			ImGui::InputFloat3("Direction", &dir.x, "%.3f", ImGuiSliderFlags_NoInput);

			auto& light = registry.get<ecs::CLight>(dirLight);
			ImGui::ColorEdit3("Color", &light.Color.x);
			ImGui::SliderFloat("Intensity", &light.Intensity, 0.0f, 10.0f);
			ImGui::ColorEdit3("Ambient Color", &light.Ambient.x);

			auto& viewpoint = registry.get<ecs::CViewpoint>(dirLight);
			if (auto* ortographic = std::get_if<ecs::CViewpoint::Orthographic>(&viewpoint.Projection))
			{
				ImGui::SliderFloat("Width", &ortographic->Width, 0.0f, 100.0f);
				ImGui::SliderFloat("Height", &ortographic->Height, 0.0f, 100.0f);
			}

			float yawDegrees = glm::degrees(transform.Rotation.x);
			ImGui::SliderFloat("Yaw", &yawDegrees, 0.0f, 360.0f, "%.3f", ImGuiSliderFlags_NoInput);
			transform.Rotation.x = glm::radians(yawDegrees);
			float pitchDegress = glm::degrees(transform.Rotation.y);
			ImGui::SliderFloat("Pitch", &pitchDegress, -90.0f, 90.0f, "%.3f", ImGuiSliderFlags_NoInput);
			transform.Rotation.y = glm::radians(pitchDegress);

			if (transformChanged)
				sceneTransform.SetTransform(transform);
			if (ImGui::Button("Reset"))
			{
				auto& controller = registry.get<ecs::CViewpointController>(dirLight);
				sceneTransform.SetTransform(controller.OriginalTransform);
				viewpoint = controller.OriginalViewpoint;
			}
			ImGui::PopID();
		}
		auto terrainView = registry.view<proc::CTerrain, proc::CErosionParameters>();
		for (auto terrainEnt : terrainView)
		{
			auto& terrain = registry.get<proc::CTerrain>(terrainEnt);
			auto& erosionParams = registry.get<proc::CErosionParameters>(terrainEnt);
			auto* entityInfo = registry.try_get<ecs::CEntityInfo>(terrainEnt);
			auto* terrainRenderable = registry.try_get<ecs::CStaticRenderable>(terrainEnt);
			auto* waterRenderable = registry.try_get<ecs::CStaticRenderable>(terrainEnt);
			if (ImGui::CollapsingHeader("Terrain_", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushID("Terrain");
				ImGui::Checkbox("With Water", &erosionParams.MeshWithWater);
				ImGui::Checkbox("Base from File", &erosionParams.BaseFromFile);
				if (!erosionParams.BaseFromFile)
				{
					ImGui::SliderFloat("Initial Roughness", &erosionParams.InitialRoughness, 0.0f, 2.0f);
					ImGui::Checkbox("Random", &erosionParams.Random);
					if (!erosionParams.Random)
						ImGui::SliderInt("Seed", &erosionParams.Seed, 0, 100000);
				}
				ImGui::SliderFloat("Min Height", &erosionParams.MinHeight, 0.0f, 100.0f);
				ImGui::SliderFloat("Max Height", &erosionParams.MaxHeight, 0.0f, 200.0f);
				ImGui::Checkbox("Erode Each Frame", &erosionParams.ErodeEachFrame);
				ImGui::SliderInt("Iterations", &erosionParams.Iterations, 1, 1024);
				ImGui::SliderFloat("Total Length", &erosionParams.TotalLength, 100.0f, 2048.0f);

				ImGui::SliderFloat("Rain Rate", &erosionParams.RainRate, 0.0f, 0.1f);
				ImGui::SliderFloat("Pipe Cross Section", &erosionParams.PipeCrossSection, 0.0f, 100.0f);
				ImGui::SliderFloat("Evaporation Rate", &erosionParams.EvaporationRate, 0.0f, 0.1f);
				ImGui::SliderFloat("Sediment Capacity", &erosionParams.SedimentCapacity, 0.0f, 2.0f);
				ImGui::SliderFloat("Soil Suspension Rate", &erosionParams.SoilSuspensionRate, 0.0f, 2.f);
				ImGui::SliderFloat("Sediment Deposition Rate", &erosionParams.SedimentDepositionRate, 0.0f, 3.0f);
				ImGui::SliderFloat("Soil Hardening Rate", &erosionParams.SoilHardeningRate, 0.0f, 2.0f);
				ImGui::SliderFloat("Soil Softening Rate", &erosionParams.SoilSofteningRate, 0.0f, 2.0f);
				ImGui::SliderFloat("Minimum Soil Softness", &erosionParams.MinimumSoilSoftness, 0.0f, 1.0f);
				ImGui::SliderFloat("Maximal Erosion Depth", &erosionParams.MaximalErosionDepth, 0.0f, 40.0f);

				ImGui::SliderFloat("Softness Talus Coefficient", &erosionParams.SoftnessTalusCoefficient, 0.0f, 1.0f);
				ImGui::SliderFloat("Min Talus Coefficient", &erosionParams.MinTalusCoefficient, 0.0f, 1.0f);
				ImGui::SliderFloat("Thermal Erosion Rate", &erosionParams.ThermalErosionRate, 0.0f, 5.0f);

				ImGui::PopID();
			}
		}

		if (ImGui::CollapsingHeader("Texture View"))
		{

			if (ImGui::BeginCombo("Textures", renderer.ViewingTexture ? renderer.ViewingTexture->c_str() : "None"))
			{
				for (auto& [name, func] : renderer.ViewableTextures)
				{
					if (ImGui::Selectable(name.c_str()))
						renderer.ViewingTexture = name;
				}
				if (ImGui::Selectable("None"))
				{
					renderer.ViewingTexture = std::nullopt;
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::TreeNodeEx("Entities", ImGuiTreeNodeFlags_Framed))
		{
			auto view = registry.view<CSceneTransform>();
			for (auto entity : view)
			{
				auto& transform = view.get<CSceneTransform>(entity);
				if (transform.Parent == nullptr)
					UIDrawMeshTree(registry, entity);
			}
			ImGui::TreePop();
		}
		ImGui::End();
	}

	ImGui::Render();
}
}; // namespace rad::ecs
