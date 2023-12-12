#include "AssetLoader.h"

namespace dfr
{
ru<Mesh> loadObj(std::string_view cPath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, cPath.data());
	assert(ret && !shapes.empty());
	auto& shape1 = shapes[0];
	ru<Mesh> mesh(new Mesh());
	mesh->Indices = std::move(shape1.mesh.indices);
	mesh->Positions = std::move(attrib.vertices);
	mesh->Normals = std::move(attrib.normals);
	mesh->TexCoords = std::move(attrib.texcoords);
	return mesh;
}
};