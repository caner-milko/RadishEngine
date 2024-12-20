#include "ModelManager.h"

#include "Renderer.h"
#include <filesystem>
#include <tiny_obj_loader.h>
#include "TextureManager.h"

// Hash function for Vertex
namespace std
{
template <> struct hash<glm::vec2>
{
	size_t operator()(glm::vec2 const& v) const
	{
		size_t seed = 0;
		rad::HashCombine(seed, v.x, v.y);
		return seed;
	}
};
template <> struct hash<glm::vec3>
{
	size_t operator()(glm::vec3 const& v) const
	{
		size_t seed = 0;
		rad::HashCombine(seed, v.x, v.y, v.z);
		return seed;
	}
};
template <> struct hash<rad::Vertex>
{
	size_t operator()(rad::Vertex const& vertex) const
	{
		size_t seed = 0;
		rad::HashCombine(seed, vertex.Position, vertex.Normal, vertex.TexCoord);
		return seed;
	}
};
} // namespace std

namespace rad
{

void LoadVerticesAndIndexBuffer(const tinyobj::ObjReader& reader, std::vector<Vertex>& vertices,
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

OptionalRef<ObjModel> ModelManager::LoadModel(const std::string& modelPath, CommandContext& commandCtx)
{
	auto it = Models.find(modelPath);
	if (it != Models.end())
	{
		return it->second;
	}

	auto& objModel = Models[modelPath] = ObjModel{};

	tinyobj::ObjReaderConfig readerConfig;
	tinyobj::ObjReader reader;
	reader.ParseFromFile(modelPath, readerConfig);
	assert(reader.Valid());
	auto& attrib = reader.GetAttrib();
	auto shapes = reader.GetShapes();

	objModel.Meshes.reserve(shapes.size());
	objModel.Materials.reserve(reader.GetMaterials().size());

	std::vector<Vertex> vertices;
	std::vector<std::vector<uint32_t>> indicesPerShape;
	LoadVerticesAndIndexBuffer(reader, vertices, indicesPerShape);
	objModel.Vertices =
		DXTypedBuffer<Vertex>::CreateAndUpload(Renderer.GetDevice(), s2ws(modelPath) + L"_Vertices", commandCtx,
											   vertices, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	for (auto& mat : reader.GetMaterials())
	{
		auto& material = objModel.Materials[mat.name];
		material.Name = mat.name;

		material.MaterialInfo = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

		rad::hlsl::MaterialBuffer matInfo = {};
		bool difTexLoaded = false;
		// Load the textures
		if (!mat.diffuse_texname.empty())
		{
			material.DiffuseTextureName =
				std::filesystem::path(modelPath).parent_path().string() + "/" + mat.diffuse_texname;
			// Load texture into memory
			auto* tex = Renderer.TextureManager->LoadTexture(std::filesystem::path(*material.DiffuseTextureName), {},
															 commandCtx, true);
			if (tex)
			{
				material.DiffuseTextureSRV =
					g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Texture2D.MipLevels = -1;
				tex->CreatePlacedSRV(material.DiffuseTextureSRV->GetView(0), &srvDesc);
				difTexLoaded = true;
				matInfo.DiffuseTextureIndex = material.DiffuseTextureSRV->Index;
			}
		}
		if (!difTexLoaded)
		{
			matInfo.Diffuse = glm::vec4{mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0f};
			material.DiffuseColor = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
		}
		if (!mat.displacement_texname.empty())
		{
			material.NormalMapTextureName =
				std::filesystem::path(modelPath).parent_path().string() + "/" + mat.displacement_texname;
			// Load texture into memory
			auto* tex = Renderer.TextureManager->LoadTexture(std::filesystem::path(*material.NormalMapTextureName), {},
															 commandCtx, true);
			if (tex)
			{
				material.NormalMapTextureSRV =
					g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Texture2D.MipLevels = -1;
				tex->CreatePlacedSRV(material.NormalMapTextureSRV->GetView(0), &srvDesc);
				matInfo.NormalMapTextureIndex = material.NormalMapTextureSRV->Index;
			}
		}

		material.MaterialInfoBuffer = DXTypedSingularBuffer<rad::hlsl::MaterialBuffer>::CreateAndUpload(
			Renderer.GetDevice(), s2ws(mat.name) + L"_MaterialInfo", commandCtx, matInfo,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		material.MaterialInfoBuffer.CreatePlacedCBV(material.MaterialInfo.GetView(0));
	}

	// Loop over shapes
	for (size_t i = 0; i < shapes.size(); i++)
	{
		auto& shape = shapes[i];
		auto& indices = indicesPerShape[i];
		auto& mesh = objModel.Meshes[shape.name];
		mesh.Model = objModel.Vertices;
		mesh.Name = shape.name;
		mesh.Indices = DXTypedBuffer<uint32_t>::CreateAndUpload(Renderer.GetDevice(), s2ws(shape.name), commandCtx,
																indices, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		mesh.Material = objModel.Materials[reader.GetMaterials()[shape.mesh.material_ids[0]].name];
	}

	return objModel;
}

} // namespace rad