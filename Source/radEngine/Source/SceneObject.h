#pragma once

#include "Model.h"
#include "RendererCommon.h"

namespace rad
{
struct MeshObject
{
	std::string Name;
	Vector4 Position = { 0, 0, 0, 1 };
	Vector4 Rotation = { 0, 0, 0, 0 };
	Vector4 Scale = { 1, 1, 1, 0 };

	MeshObject* Parent = nullptr;
	std::vector<MeshObject> Children;
	IndexedModel* IndexedModel = nullptr;
	Material* Material = nullptr;
	bool TransformOnly = true;

	MeshObject(std::string name)
		: Name(std::move(name)), TransformOnly(true)
	{
	}

	MeshObject(std::string name, rad::IndexedModel* indexedModel, rad::Material* material)
		: Name(std::move(name)), IndexedModel(indexedModel), Material(material), TransformOnly(false)
	{
	}

	Matrix4x4 LocalModelMatrix() const
	{
		return DirectX::XMMatrixAffineTransformation(Scale, { 0, 1, 0, 0 }, DirectX::XMQuaternionRotationRollPitchYawFromVector(Rotation), Position);
	}

	bool IsRenderable() const
	{
		return !TransformOnly && IndexedModel && Material;
	}

	Renderable ToRenderable(Matrix4x4 parentModel = DirectX::XMMatrixIdentity()) const
	{
		assert(!TransformOnly);
		Renderable renderable{};
		renderable.Name = Name;
		renderable.GlobalModelMatrix = XMMatrixMultiply(parentModel, LocalModelMatrix());
		renderable.MaterialInfo = Material->MaterialInfo.GetGPUHandle();
		if (Material->DiffuseTextureSRV)
			renderable.DiffuseTextureSRV = Material->DiffuseTextureSRV->GetGPUHandle();
		if (Material->NormalMapTextureSRV)
			renderable.NormalMapTextureSRV = Material->NormalMapTextureSRV->GetGPUHandle();
		renderable.VertexBufferView = IndexedModel->Model->VertexBufferView;
		renderable.IndexBufferView = IndexedModel->IndexBufferView;
		return renderable;
	}
};
}