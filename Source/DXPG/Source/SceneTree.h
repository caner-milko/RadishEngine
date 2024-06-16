#pragma once

#include "DXPGCommon.h"
#include "SceneObject.h"

namespace dxpg
{

struct SceneTree
{
	MeshObject Root{ "Root" };

	std::vector<Renderable> SceneToRenderableList() const;

	MeshObject* AddObject(MeshObject const& object, MeshObject* parent = nullptr);

};

}