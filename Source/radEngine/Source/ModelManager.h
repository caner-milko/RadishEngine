#pragma once

#include "RadishCommon.h"
#include "DXHelpers.h"

#include "Model.h"

#include "RendererCommon.h"

namespace rad
{

struct ObjModel
{
	VertexBuffer Vertices;
	std::unordered_map<std::string, IndexedModel> ModelViews;
	std::unordered_map<std::string, Material> Materials;
	std::vector<std::pair<IndexedModel*, Material*>> Objects;
};

struct ModelManager : Singleton<ModelManager>
{
	void Init(ID3D12Device* device);
	ObjModel* LoadModel(const std::string& modelPath, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList);

	ID3D12Device* Device = nullptr;
	std::unordered_map<std::string, std::unique_ptr<ObjModel>> Models;
};
}