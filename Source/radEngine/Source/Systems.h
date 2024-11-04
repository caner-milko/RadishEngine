#pragma once

#include "Graphics/Model.h"
#include <entt/entt.hpp>
#include "RadishCommon.h"
#include <glm/glm.hpp>

namespace rad::ecs
{
template<typename T>
struct EntityComponentView
{
entt:entity Entity = entt::null;
	T& GetComponent(entt::registry& registry) const
	{
		return registry.get<T>(Entity);
	}

	T& GetComponent(entt::registry const& registry) const
	{
		return registry.get<T>(Entity);
	}
};

struct CEntityInfo
{
	std::string Name;
};
struct CSceneHierarchy
{
	entt::entity Parent = entt::null;
	std::vector<entt::entity> Children;
};
struct CTransform
{
	glm::vec3 Translation = glm::vec3(0.0f);
	glm::vec3 Rotation = glm::vec3(0.0f);
	glm::vec3 Scale = glm::vec3(1.0f);
	
	glm::vec4 GetModelMatrix() const;
};
struct CStaticModelVertices
{
	DXTypedBuffer<Vertex> Vertices;
};
struct CStaticModelIndices
{
	DXTypedBuffer<uint32_t> Indices;
};
struct CStaticRenderable
{
	entt::entity Vertices = entt::null;
	entt::entity Indices = entt::null;

};
}