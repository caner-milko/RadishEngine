#include "ModelManager.h"

#include "Renderer.h"
#include <filesystem>
#include <tiny_obj_loader.h>
#include "TextureManager.h"
#include "Graphics/RenderGraphHelpers.h"

// Hash function for Vertex
namespace std
{
template <>
struct hash<glm::vec2>
{
	size_t operator()(glm::vec2 const& v) const { return rad::HashCombine(v.x, v.y); }
};
template <>
struct hash<glm::vec3>
{
	size_t operator()(glm::vec3 const& v) const { return rad::HashCombine(v.x, v.y, v.z); }
};
template <>
struct hash<rad::Vertex>
{
	size_t operator()(rad::Vertex const& vertex) const
	{
		return rad::HashCombine(vertex.Position, vertex.Normal, vertex.TexCoord);
	}
};
} // namespace std

namespace rad
{

void LoadVerticesAndIndexBuffer(const tinyobj::ObjReader& reader,
								std::vector<Vertex>& vertices,
								std::vector<std::vector<uint32_t>>& indexPerShape)
{
	auto& attrib = reader.GetAttrib();
	auto shapes = reader.GetShapes();

	std::unordered_map<Vertex, size_t> uniqueVertices;

	for (size_t s = 0; s < shapes.size(); s++)
	{
		auto& indices = indexPerShape.emplace_back();
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			// hardcode loading to triangles
			int fv = 3;
			Vertex newVertices[3];
			glm::vec3 bitangent;
			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++)
			{
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				// vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				// vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				// vertex texcoords
				tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];

				// copy it into our vertex
				Vertex new_vert;
				new_vert.Position = {vx, vy, vz};

				new_vert.Normal = {nx, ny, nz};

				new_vert.TexCoord = {tx, ty};

				newVertices[v] = new_vert;
			}

			glm::vec3 deltaPos1 = newVertices[1].Position - newVertices[0].Position;
			glm::vec3 deltaPos2 = newVertices[2].Position - newVertices[0].Position;

			glm::vec2 deltaUV1 = newVertices[1].TexCoord - newVertices[0].TexCoord;
			glm::vec2 deltaUV2 = newVertices[2].TexCoord - newVertices[0].TexCoord;

			float r = 1.0F / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
			glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
			bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;
			newVertices[0].Tangent = tangent;
			newVertices[1].Tangent = tangent;
			newVertices[2].Tangent = tangent;

			for (size_t v = 0; v < fv; v++)
			{
				Vertex& vtx = newVertices[v];

				glm::vec3 n = vtx.Normal;
				glm::vec3 t = vtx.Tangent;

				// Gram-Schmidt orthogonalize
				t = glm::normalize(t - n * glm::dot(n, t));

				// Calculate handedness
				if (glm::dot(glm::cross(n, t), bitangent) < 0.0f)
					t *= -1;

				vtx.Tangent = -t;
			}

			for (size_t v = 0; v < fv; v++)
			{
				Vertex& vtx = newVertices[v];
				size_t index;
				if (auto it = uniqueVertices.find(vtx); it != uniqueVertices.end())
				{
					index = it->second;
					Vertex& existing = vertices[index];
					existing.Tangent = existing.Tangent + vtx.Tangent;
				}
				else
				{
					index = vertices.size();
					vertices.emplace_back(vtx);
					uniqueVertices.emplace(vtx, index);
				}
				indices.push_back(index);
			}

			index_offset += fv;
		}
	}
}

OptionalRef<ObjModel> ModelManager::LoadModel(const std::string& modelPath, RenderGraphBuilder& rgBuilder)
{
	auto it = Models.find(modelPath);
	if (it != Models.end())
	{
		return it->second;
	}

	tinyobj::ObjReaderConfig readerConfig;
	tinyobj::ObjReader reader;
	reader.ParseFromFile(modelPath, readerConfig);
	assert(reader.Valid());
	auto& attrib = reader.GetAttrib();
	auto shapes = reader.GetShapes();

	std::vector<Vertex> vertices;
	std::vector<std::vector<uint32_t>> indicesPerShape;
	LoadVerticesAndIndexBuffer(reader, vertices, indicesPerShape);

	ResourcePool::OwnedResource& verticesBuf = Renderer.ResourcePool->GetResource(
		ResourceCreateHelper::Buffer(sizeof(Vertex) * vertices.size(), ResourcePresetFlags::VertexBuffer),
		modelPath + "_Vertices");

	{
		auto rgVerticesBuf = rgBuilder.GetOrAddExternalResource(verticesBuf.AsView());
		rghelpers::UploadBufferData(rgBuilder, rgVerticesBuf, std::move(vertices));
	}

	auto& objModel = Models.insert_or_assign(modelPath, ObjModel{.Vertices = verticesBuf}).first->second;
	objModel.Meshes.reserve(shapes.size());
	objModel.Materials.reserve(reader.GetMaterials().size());

	for (auto& mat : reader.GetMaterials())
	{
		std::optional<std::string> diffuseTexName = std::nullopt;
		std::optional<std::string> normalMapTexName = std::nullopt;
		auto& materialInfoBuf = Renderer.ResourcePool->GetResource(
			ResourceCreateHelper::Buffer(sizeof(rad::hlsl::MaterialBuffer), ResourcePresetFlags::ConstantBuffer),
			mat.name + "_MaterialInfo");
		Material material{.Name = mat.name, .MaterialInfoBuffer = materialInfoBuf};
		rad::hlsl::MaterialBuffer matInfo = {};
		bool difTexLoaded = false;
		// Load the textures
		if (!mat.diffuse_texname.empty())
		{
			diffuseTexName = std::filesystem::path(modelPath).parent_path().string() + "/" + mat.diffuse_texname;
			material.DiffuseTextureName = diffuseTexName;
			// Load texture into memory
			if (auto* tex = Renderer.TextureManager->LoadTexture(
					std::filesystem::path(*material.DiffuseTextureName), {}, rgBuilder, true))
			{
				material.DiffuseTexture = tex->operator rad::ResourcePool::Resource&();
				difTexLoaded = true;
			}
		}
		if (!difTexLoaded)
		{
			matInfo.Diffuse = glm::vec4{mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0f};
			material.DiffuseColor = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
		}
		if (!mat.displacement_texname.empty())
		{
			normalMapTexName = std::filesystem::path(modelPath).parent_path().string() + "/" + mat.displacement_texname;
			material.NormalMapTextureName = normalMapTexName;
			// Load texture into memory
			if (auto* tex = Renderer.TextureManager->LoadTexture(
					std::filesystem::path(*material.NormalMapTextureName), {}, rgBuilder, true))
			{
				material.NormalMapTexture = tex->operator rad::ResourcePool::Resource&();
			}
		}

		{
			auto rgMatInfoBuf = rgBuilder.GetOrAddExternalResource(materialInfoBuf.AsView());
			rghelpers::UploadConstantBufferData(rgBuilder, rgMatInfoBuf, matInfo);
		}
		objModel.Materials.insert_or_assign(mat.name, std::move(material));
	}

	// Loop over shapes
	for (size_t i = 0; i < shapes.size(); i++)
	{
		auto& shape = shapes[i];
		auto& indices = indicesPerShape[i];
		auto& indexBuf = Renderer.ResourcePool->GetResource(
			ResourceCreateHelper::Buffer(sizeof(uint32_t) * indices.size(), ResourcePresetFlags::IndexBuffer),
			shape.name + "_Indices");
		{
			auto rgIndexBuf = rgBuilder.GetOrAddExternalResource(indexBuf.AsView());
			rghelpers::UploadBufferData(rgBuilder, rgIndexBuf, std::move(indices));
		}
		objModel.Meshes.insert_or_assign(
			shape.name,
			Mesh{
				.Name = shape.name,
				.Vertices = objModel.Vertices,
				.Indices = indexBuf,
				.Material = objModel.Materials.at(reader.GetMaterials()[shape.mesh.material_ids[0]].name),
			});
	}

	return objModel;
}

} // namespace rad