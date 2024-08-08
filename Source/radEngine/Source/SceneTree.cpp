#include "SceneTree.h"

namespace rad
{


std::vector<Renderable> SceneTree::SceneToRenderableList() const
{
	std::vector<Renderable> renderables;
	std::stack<Matrix4x4> matrixStack;
	std::stack<std::pair<MeshObject const*, uint32_t>> objectStackAndDepth;
	objectStackAndDepth.push({ &Root, 1 });
	matrixStack.push(DirectX::XMMatrixIdentity());
	uint32_t lastDepth = 1;
	while (!objectStackAndDepth.empty())
	{
		auto [current, depth] = objectStackAndDepth.top();
		objectStackAndDepth.pop();
		if (depth < lastDepth)
		{
			for (uint32_t i = 0; i < lastDepth - depth; ++i)
				matrixStack.pop();
		}
		assert(matrixStack.size() == depth);
		lastDepth = depth;
		auto parentMatrix = matrixStack.top();
		Matrix4x4 globalModelMatrix;
		if (current->IsRenderable())
		{
			const Renderable& renderable = current->ToRenderable(parentMatrix);
			renderables.push_back(renderable);
			globalModelMatrix = renderable.GlobalModelMatrix;
		}
		else
			globalModelMatrix = XMMatrixMultiply(parentMatrix, current->LocalModelMatrix());
		if (current->Children.size() > 0)
		{
			matrixStack.push(globalModelMatrix);
			for (auto& child : current->Children)
				objectStackAndDepth.push({ &child, depth + 1 });
		}
	}
	return renderables;
}

MeshObject* SceneTree::AddObject(MeshObject const& object, MeshObject* parent)
{
	if (!parent)
		parent = &Root;
	auto& newObj = parent->Children.emplace_back(object);
	newObj.Parent = parent;
	return &newObj;
}

}