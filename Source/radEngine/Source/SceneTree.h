#pragma once

#include "RadishCommon.h"
#include "SceneObject.h"

namespace rad
{

struct SceneTree
{
	MeshObject Root{ "Root" };

	std::vector<Renderable> SceneToRenderableList() const;

	MeshObject* AddObject(MeshObject const& object, MeshObject* parent = nullptr);

};

}