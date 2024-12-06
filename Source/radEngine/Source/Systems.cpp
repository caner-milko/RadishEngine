#include "Systems.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace rad::ecs
{
glm::mat4 Transform::GetModelMatrix(glm::mat4 parentWorldMatrix) const
{
	parentWorldMatrix = glm::translate(parentWorldMatrix, Position);
	parentWorldMatrix = glm::toMat4(glm::quat(Rotation)) * parentWorldMatrix;
	parentWorldMatrix = glm::scale(parentWorldMatrix, Scale);
	return parentWorldMatrix;
}

bool CStaticRenderSystem::Init(ID3D12Device& device)
{
	return false;
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
		renderData.Vertices = renderable.Vertices;
		renderData.Indices = renderable.Indices;
		renderData.Material = renderable.Material.MaterialInfoBuffer;

		renderObjects.push_back(renderData);
	}

	frameRecord.Push(TypedRenderCommand<StaticRenderData>{.Name = "StaticRender", .Data = renderObjects, 
		.DepthOnlyPass = [this](auto span, auto view, auto passData) {DepthOnlyPass(span, view, passData); },
		.DeferredPass = [this](auto span, auto view, auto passData) {DeferredPass(span, view, passData); }
	});
}
void CStaticRenderSystem::DepthOnlyPass(std::span<StaticRenderData> renderObjects, const RenderView& view, DepthOnlyPassData& passData)
{
}
void CStaticRenderSystem::DeferredPass(std::span<StaticRenderData> renderObjects, const RenderView& view, DeferredPassData& passData)
{
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
