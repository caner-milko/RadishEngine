#pragma once

#include "RadishCommon.h"
#include "DXHelpers.h"

#include "Model.h"

#include "RendererCommon.h"

namespace rad
{

struct ObjModel;

struct Mesh
{
	std::string Name;
	OptionalRef<DXTypedBuffer<Vertex>> Model = std::nullopt;
	DXTypedBuffer<uint32_t> Indices;
	OptionalRef<Material> Material;
};

struct ObjModel
{
	DXTypedBuffer<Vertex> Vertices;
	std::unordered_map<std::string, Mesh> Meshes;
	std::unordered_map<std::string, Material> Materials;
};

struct ModelManager
{
	ModelManager(Renderer& renderer) : Renderer(renderer) {}
	OptionalRef<ObjModel> LoadModel(const std::string& modelPath, CommandContext& commandContext);

	Renderer& Renderer;
	std::unordered_map<std::string, ObjModel> Models;
};
} // namespace rad