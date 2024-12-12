#pragma once

#include "Graphics/Model.h"
#include <entt/entt.hpp>
#include "RadishCommon.h"
#include "ConstantBuffers.hlsli"
#include "Graphics/Model.h"
#include "Graphics/Renderer.h"
#include "Graphics/Model.h"
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

struct CSceneTransform
{
	struct WorldTransform
	{
		glm::mat4 WorldMatrix = glm::mat4(1.0f);
		// For directx, so row-major
		glm::vec3 GetPosition() const { return glm::vec3(WorldMatrix[3]); }
		glm::vec3 GetScale() const { return glm::vec3(glm::length(glm::vec3(WorldMatrix[0])), glm::length(glm::vec3(WorldMatrix[1])), glm::length(glm::vec3(WorldMatrix[2]))); }
		glm::vec3 GetForward() const { return glm::mat3(WorldMatrix) * glm::vec3(0, 0, 1.0f); }
		glm::vec3 GetRight() const { return glm::mat3(WorldMatrix) * glm::vec3(-1.0f, 0, 0);	}
		glm::vec3 GetUp() const { return glm::mat3(WorldMatrix) * glm::vec3(0, 1.0f, 0); }
		glm::mat3 GetRotation() const { return glm::mat3(WorldMatrix); }
		operator ecs::Transform() const;
	};
	CSceneTransform* Parent = nullptr;
	std::vector<CSceneTransform*> Children;


	WorldTransform GetWorldTransform() const
	{
		WorldTransform parentWorldTransform = GetParentWorldTransform();
		return WorldTransform{ .WorldMatrix = Transform.GetModelMatrix(parentWorldTransform.WorldMatrix) };
	}

	ecs::Transform const& LocalTransform() const { return Transform; }

	void SetParent(CSceneTransform* parent)
	{
		if (Parent != nullptr)
		{
			std::erase_if(Parent->Children, [this](auto child) { return child == this; });
		}
		Parent = parent;
		if (parent != nullptr)
		{
			parent->Children.push_back(this);
		}
		InvalidateParentTransform();
	}

	void SetTransform(const ecs::Transform& transform)
	{
		Transform = transform;
		InvalidateChildTransforms();
	}

private:
	Transform Transform;
	mutable std::optional<WorldTransform> CachedParentWorldTransform;

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
};

struct CStaticRenderable
{
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
	glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
	float Intensity = 1.0f;
	glm::vec3 Ambient = { 0.1f, 0.1f, 0.1f };
};
struct CLightSystem
{
	void Update(entt::registry& registry, RenderFrameRecord& frameRecord);
};
struct CViewpointController
{
	CViewpointController(Transform transform, CViewpoint viewpoint) : OriginalTransform(transform), OriginalViewpoint(std::move(viewpoint)) {}
	Transform OriginalTransform;
	CViewpoint OriginalViewpoint;
	float MoveSpeed = 1.0f;
	float RotateSpeed = 1.0f;
};
struct CViewpointControllerSystem
{
	entt::entity ActiveViewpoint = entt::null;
	void Update(entt::registry& registry, InputManager& io, float deltaTime);
};

struct CUISystem
{
	void Init(Renderer& renderer, SDL_Window* window);
	void Destroy();
	void Update(entt::registry& registry, RenderFrameRecord& frameRecord);
};

struct CTerrain
{
	std::shared_ptr<DXTexture> HeightMap;
	std::shared_ptr<DXTexture> WaterHeightMap;
};
struct CTerrainErodable
{
	bool Erode = false;
	std::shared_ptr<DXTexture> WaterOutflux;
	std::shared_ptr<DXTexture> TempR32;
	std::shared_ptr<DXTexture> HardnessMap;
	std::shared_ptr<DXTexture> SedimentMap;
	std::shared_ptr<DXTexture> ThermalOutflux1, ThermalOutflux2;

	bool RegenerateBase = false;
	bool Random = false;
	int Seed = 0;
	bool BaseFromFile = false;
	float InitialRoughness = 4.0f;
	float MinHeight = 0.0f;
	float MaxHeight = 120.0f;
	int Iterations = 1;
	float RainRate = 0.015f;
	float EvaporationRate = 0.006f;
	float TotalLength = 1024.0;
	float PipeCrossSection = 20.0f;
	float SedimentCapacity = 1.0f;
	float SoilSuspensionRate = 0.6f;
	float SedimentDepositionRate = 0.8f;
	float SoilHardeningRate = 0.2f;
	float SoilSofteningRate = 0.2f;
	float MinimumSoilSoftness = 0.0f;
	float MaximalErosionDepth = 10.0f;

	float SoftnessTalusCoefficient = 0.6f;
	float MinTalusCoefficient = 0.3f;
	float ThermalErosionRate = 0.1f;
};
struct CTerrainRenderable
{
	//std::shared_ptr<DXTypedSingularBuffer<hlsl::TerrainMaterial>> Material;
};
struct CShallowWaterRenderable
{
	//std::shared_ptr<DXTypedSingularBuffer<hlsl::WaterMaterial>> Material;
};
}