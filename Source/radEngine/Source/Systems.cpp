#include "Systems.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"
#include "imgui.h"

namespace rad::ecs
{
glm::mat4 Transform::GetModelMatrix(glm::mat4 parentWorldMatrix) const
{
	parentWorldMatrix = glm::translate(parentWorldMatrix, Position);
	parentWorldMatrix = parentWorldMatrix * glm::toMat4(glm::quat(Rotation));
	parentWorldMatrix = glm::scale(parentWorldMatrix, Scale);
	return parentWorldMatrix;
}
CSceneTransform::WorldTransform::operator ecs::Transform() const
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
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		auto [vertexShader, pixelShader] = renderer.ShaderManager->CompileBindlessGraphicsShader(L"Triangle", RAD_SHADERS_DIR L"Graphics/StaticMesh.hlsl");

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 2;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvFormats.RTFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		pipelineStateStream.RTVFormats = rtvFormats;

		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		StaticMeshPipelineState = PipelineState::Create("StaticMeshPipeline", renderer.GetDevice(), pipelineStateStream, &renderer.ShaderManager->BindlessRootSignature);
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
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(renderer.ShaderManager->CompileBindlessVertexShader(L"ShadowMap", RAD_SHADERS_DIR L"Graphics/Shadowmap.hlsl")->Blob.Get());

		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		ShadowMapPipelineState = PipelineState::Create("ShadowMapPipeline", renderer.GetDevice(), pipelineStateStream, &renderer.ShaderManager->BindlessRootSignature);
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

	frameRecord.Push(TypedRenderCommand<StaticRenderData>{.Name = "StaticRender", .Data = renderObjects, 
		.DepthOnlyPass = [this](auto span, auto view, auto passData) {DepthOnlyPass(span, view, passData); },
		.DeferredPass = [this](auto span, auto view, auto passData) {DeferredPass(span, view, passData); }
	});
}
void CStaticRenderSystem::DepthOnlyPass(std::span<StaticRenderData> renderObjects, const RenderView& view, DepthOnlyPassData& passData)
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
void CStaticRenderSystem::DeferredPass(std::span<StaticRenderData> renderObjects, const RenderView& view, DeferredPassData& passData)
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
		return glm::perspectiveLH(glm::radians(perspective->Fov), perspective->AspectRatio, perspective->Near, perspective->Far);
	}
	else
	{
		auto orthographic = std::get<Orthographic>(Projection);
		return glm::orthoLH(-orthographic.Width / 2.0f, orthographic.Width / 2.0f, -orthographic.Height / 2.0f, orthographic.Height / 2.0f, 0.1f, 100.0f);
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

		frameRecord.LightInfo = { .View = ViewpointToRenderView(viewpoint, transform), .Color = light.Color, .Intensity = light.Intensity };
	}
}
void CViewpointControllerSystem::Update(entt::registry& registry, InputManager& io, float deltaTime)
{

	if (io.IsKeyPressed(SDL_SCANCODE_ESCAPE))
	{
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	}
	if (io.IsKeyPressed(SDL_SCANCODE_E))
	{
		//Disable imgui navigation
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
	
	if(io.IsKeyPressed(SDL_SCANCODE_TAB))
		ActiveViewpoint = ActiveViewpoint == cameraEntity ? lightEntity : cameraEntity;

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

	glm::vec3 moveDir = { float(io.IsKeyDown(SDL_SCANCODE_D)) - float(io.IsKeyDown(SDL_SCANCODE_A)), 0
		, float(io.IsKeyDown(SDL_SCANCODE_W)) - float(io.IsKeyDown(SDL_SCANCODE_S)) };

	if (glm::length(moveDir) > 0.0f)
		moveDir = glm::normalize(moveDir);

	glm::vec3 moveVec = controlledTransform.GetWorldTransform().GetRotation() * moveDir;

	moveVec.y += float(io.IsKeyDown(SDL_SCANCODE_SPACE)) - float(io.IsKeyDown(SDL_SCANCODE_LCTRL));

	if(glm::length(moveVec) > 0.0f)
		moveVec = glm::normalize(moveVec);

	auto curTransform = controlledTransform.LocalTransform();
	curTransform.Position = curTransform.Position + moveVec * deltaTime * controlledController.MoveSpeed;

	if (!io.CursorEnabled)
	{
		curTransform.Rotation = curTransform.Rotation + glm::vec3(io.Immediate.MouseDelta.y, io.Immediate.MouseDelta.x, 0) * controlledController.RotateSpeed * deltaTime;
		curTransform.Rotation.x = glm::clamp(curTransform.Rotation.x, -glm::pi<float>() * .5f + 0.0001f, glm::pi<float>() * .5f - 0.0001f);
	}
	controlledTransform.SetTransform(curTransform);
}
};
