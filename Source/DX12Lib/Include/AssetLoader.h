#pragma once

#include "Common.h"
#include <tiny_obj_loader.h>

namespace dfr
{
struct Mesh
{
	std::vector<float> Positions, TexCoords, Normals;
	std::vector<tinyobj::index_t> Indices;
};
ru<Mesh> loadObj(std::string_view cPath);
};