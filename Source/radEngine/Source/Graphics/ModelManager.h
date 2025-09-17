#pragma once

#include "EngineCommon.h"
#include "DXHelpers.h"

#include "Model.h"

#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{

struct ObjModel;

struct Mesh
{
	std::string Name;
	Ref<ResourcePool::OwnedResource> Vertices;
	Ref<ResourcePool::OwnedResource> Indices; // uint32_t
	Ref<Material> Material;
};

struct ObjModel
{
	Ref<ResourcePool::OwnedResource> Vertices; // Vertex
	std::unordered_map<std::string, Mesh> Meshes;
	std::unordered_map<std::string, Material> Materials;
};

struct ModelManager
{
	ModelManager(Renderer& renderer) : Renderer(renderer) {}
	OptionalRef<ObjModel> LoadModel(const std::string& modelPath, RenderGraphBuilder& rgBuilder);

	Renderer& Renderer;
	std::unordered_map<std::string, ObjModel> Models;
};
} // namespace rad