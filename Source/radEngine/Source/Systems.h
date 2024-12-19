#pragma once

#include <entt/entt.hpp>
#include "RadishCommon.h"
#include "ConstantBuffers.hlsli"
#include "Graphics/Model.h"
#include "Graphics/Renderer.h"
#include "Graphics/PipelineState.h"
#include "InputManager.h"

struct SDL_Window;

namespace rad::ecs
{
struct CEntityInfo
{
	std::string Name;
};

struct Transform
{
	glm::vec3 Position = glm::vec3(0.0f);
	glm::vec3 Rotation = glm::vec3(0.0f);
	glm::vec3 Scale = glm::vec3(1.0f);

	glm::mat4 GetModelMatrix(glm::mat4 parentWorldMatrix = glm::mat4(1.0f)) const;
};
struct WorldTransform
{
	glm::mat4 WorldMatrix = glm::mat4(1.0f);
	// For directx, so row-major
	glm::vec3 GetPosition() const
	{
		return glm::vec3(WorldMatrix[3]);
	}
	glm::vec3 GetScale() const
	{
		return glm::vec3(glm::length(glm::vec3(WorldMatrix[0])), glm::length(glm::vec3(WorldMatrix[1])),
						 glm::length(glm::vec3(WorldMatrix[2])));
	}
	glm::vec3 GetForward() const
	{
		return glm::mat3(WorldMatrix) * glm::vec3(0, 0, 1.0f);
	}
	glm::vec3 GetRight() const
	{
		return glm::mat3(WorldMatrix) * glm::vec3(-1.0f, 0, 0);
	}
	glm::vec3 GetUp() const
	{
		return glm::mat3(WorldMatrix) * glm::vec3(0, 1.0f, 0);
	}
	glm::mat3 GetRotation() const
	{
		return glm::mat3(WorldMatrix);
	}
	operator ecs::Transform() const;
};
struct CSceneTransform
{
	CSceneTransform(entt::entity entity) : Entity(entity) {}
	CSceneTransform* Parent = nullptr;
	std::vector<Ref<CSceneTransform>> Children;
	entt::entity Entity = entt::null;

	ecs::Transform const& LocalTransform() const
	{
		return Transform;
	}

	WorldTransform GetWorldTransform() const
	{
		WorldTransform parentWorldTransform = GetParentWorldTransform();
		return WorldTransform{.WorldMatrix = Transform.GetModelMatrix(parentWorldTransform.WorldMatrix)};
	}

	WorldTransform GetParentWorldTransform() const
	{
		if (!CachedParentWorldTransform)
		{
			if (Parent == nullptr)
				CachedParentWorldTransform = WorldTransform{};
			else
				CachedParentWorldTransform = Parent->GetWorldTransform();
		}
		return *CachedParentWorldTransform;
	}

	void SetParent(CSceneTransform* parent)
	{
		if (Parent != nullptr)
		{
			std::erase_if(Parent->Children, [this](Ref<CSceneTransform> child) { return child.Ptr() == this; });
		}
		Parent = parent;
		if (parent != nullptr)
		{
			parent->Children.push_back(*this);
		}
		InvalidateParentTransform();
	}

	void SetTransform(const ecs::Transform& transform)
	{
		Transform = transform;
		InvalidateChildTransforms();
	}

	void InvalidateParentTransform() const
	{
		CachedParentWorldTransform.reset();
		InvalidateChildTransforms();
	}

	void InvalidateChildTransforms() const
	{
		for (auto child : Children)
			child->InvalidateParentTransform();
	}

  private:
	Transform Transform;
	mutable std::optional<WorldTransform> CachedParentWorldTransform;
};

struct CStaticRenderable
{
	bool Hidden = false;
	DXTypedBuffer<Vertex> Vertices;
	DXTypedBuffer<uint32_t> Indices;
	Material Material;
};

struct CStaticRenderSystem
{
	GraphicsPipelineState<hlsl::StaticMeshResources> StaticMeshPipelineState;
	GraphicsPipelineState<hlsl::ShadowMapResources> ShadowMapPipelineState;
	bool Init(Renderer& renderer);
	void Update(entt::registry& registry, RenderFrameRecord& frameRecord);

	struct StaticRenderData
	{
		glm::mat4 WorldMatrix;
		uint32_t IndexCount;
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
		DescriptorAllocationView Material;
	};
	void DepthOnlyPass(std::span<StaticRenderData> renderObjects, const RenderView& view, DepthOnlyPassData& passData);
	void DeferredPass(std::span<StaticRenderData> renderObjects, const RenderView& view, DeferredPassData& passData);
};
struct CViewpoint
{
	struct Orthographic
	{
		float Width = 1.0f;
		float Height = 1.0f;
	};
	struct Perspective
	{
		float Fov = 45.0f;
		float Near = 0.1f;
		float Far = 100.0f;
		float AspectRatio = 16.0f / 9.0f;
	};
	std::variant<Orthographic, Perspective> Projection;

	glm::mat4 ViewMatrix(CSceneTransform const& sceneTransform) const;
	glm::mat4 ProjectionMatrix() const;
};
struct CCamera
{
};
struct CCameraSystem
{
	void Update(entt::registry& registry, RenderFrameRecord& frameRecord);
};
struct CLight
{
	glm::vec3 Color = {1.0f, 1.0f, 1.0f};
	float Intensity = 1.0f;
	glm::vec3 Ambient = {0.1f, 0.1f, 0.1f};
};
struct CLightSystem
{
	void Update(entt::registry& registry, RenderFrameRecord& frameRecord);
};
struct CViewpointController
{
	CViewpointController(Transform transform, CViewpoint viewpoint)
		: OriginalTransform(transform), OriginalViewpoint(std::move(viewpoint))
	{
	}
	Transform OriginalTransform;
	CViewpoint OriginalViewpoint;
	float MoveSpeed = 5.0f;
	float RotateSpeed = 2.0f;
};
struct CViewpointControllerSystem
{
	entt::entity ActiveViewpoint = entt::null;
	void Update(entt::registry& registry, InputManager& io, float deltaTime, Renderer& renderer);
};

struct CUISystem
{
	void Init(Renderer& renderer, SDL_Window* window);
	void Destroy();
	void ProcessEvent(const SDL_Event& event);
	void Update(entt::registry& registry, Renderer& renderer);
};
} // namespace rad::ecs