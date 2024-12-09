#include "Systems.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"

namespace rad::ecs
{
glm::mat4 Transform::GetModelMatrix(glm::mat4 parentWorldMatrix) const
{
	parentWorldMatrix = glm::translate(parentWorldMatrix, Position);
	parentWorldMatrix = glm::toMat4(glm::quat(Rotation)) * parentWorldMatrix;
	parentWorldMatrix = glm::scale(parentWorldMatrix, Scale);
	return parentWorldMatrix;
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
		shadowMapResources.MVP = renderObj.WorldMatrix * view.ViewProjectionMatrix;
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
		staticMeshResources.MVP = renderObj.WorldMatrix * view.ViewProjectionMatrix;
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
		return glm::perspective(perspective->Fov, perspective->AspectRatio, perspective->Near, perspective->Far);
	}
	else
	{
		auto orthographic = std::get<Orthographic>(Projection);
		return glm::ortho(-orthographic.Width / 2.0f, orthographic.Width / 2.0f, -orthographic.Height / 2.0f, orthographic.Height / 2.0f);
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
};
