#include "ModelManager.h"

#include <filesystem>
#include <tiny_obj_loader.h>
#include "TextureManager.h"

namespace dxpg
{
std::unique_ptr<ModelManager> ModelManager::Instance = nullptr;
void ModelManager::Init(ID3D12Device* device)
{
	Device = device;
}

ObjModel* ModelManager::LoadModel(const std::string& modelPath, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList)
{
	auto it = Models.find(modelPath);
	if (it != Models.end())
	{
		return it->second.get();
	}
    
	auto& objModel = Models[modelPath] = std::make_unique<ObjModel>();

    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader reader;
    reader.ParseFromFile(modelPath, readerConfig);
    assert(reader.Valid());
    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    objModel->ModelViews.reserve(shapes.size());
	objModel->Materials.reserve(reader.GetMaterials().size());

    {
		auto& model = objModel->Model;

	    model.PositionsBuffer = DXTypedBuffer<Vector3>::CreateAndUpload(Device, s2ws(modelPath) + L"_Positions", cmdList, frameCtx.IntermediateResources.emplace_back(), std::span{ (Vector3 const*)attrib.vertices.data(), attrib.vertices.size() / (sizeof(Vector3)/sizeof(tinyobj::real_t))}, D3D12_RESOURCE_STATE_GENERIC_READ);
	    model.NormalsBuffer = DXTypedBuffer<Vector3>::CreateAndUpload(Device, s2ws(modelPath) + L"_Normals", cmdList, frameCtx.IntermediateResources.emplace_back(), std::span{ (Vector3 const*)attrib.normals.data(), attrib.normals.size() / (sizeof(Vector3) / sizeof(tinyobj::real_t)) }, D3D12_RESOURCE_STATE_GENERIC_READ);
	    model.TexCoordsBuffer = DXTypedBuffer<Vector2>::CreateAndUpload(Device, s2ws(modelPath) + L"_TexCoords", cmdList, frameCtx.IntermediateResources.emplace_back(), std::span{ (Vector2 const*)attrib.texcoords.data(), attrib.texcoords.size() / (sizeof(Vector2) / sizeof(tinyobj::real_t)) }, D3D12_RESOURCE_STATE_GENERIC_READ);

	    model.VertexSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3);
        model.PositionsBuffer.CreatePlacedSRV(model.VertexSRV.GetView(0), attrib.vertices.size());
		model.NormalsBuffer.CreatePlacedSRV(model.VertexSRV.GetView(1), attrib.normals.size());
		model.TexCoordsBuffer.CreatePlacedSRV(model.VertexSRV.GetView(2), attrib.texcoords.size());
    }


    for (auto& mat : reader.GetMaterials())
    {
        auto& material = objModel->Materials[mat.name];
        material.Name = mat.name;

        material.MaterialInfo = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

		HLSL_ShaderMaterialInfo matInfo = {};
        bool difTexLoaded = false;
        // Load the textures
        if (!mat.diffuse_texname.empty())
        {
            material.DiffuseTextureName = std::filesystem::path(modelPath).parent_path().string() + "/" + mat.diffuse_texname;
            // Load texture into memory
            auto* tex = TextureManager::Get().LoadTexture(std::filesystem::path(*material.DiffuseTextureName), {}, frameCtx, cmdList, true);
            if (tex)
            {
		        material.DiffuseTextureSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Texture2D.MipLevels = -1;
                tex->CreatePlacedSRV(material.DiffuseTextureSRV->GetView(0), &srvDesc);
				difTexLoaded = true;
				matInfo.UseDiffuseTexture = 1;
            }
        }
        if (!difTexLoaded)
        {
            matInfo.Diffuse = Vector4{ mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0f };
            material.DiffuseColor = Vector3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
        }

        material.MaterialInfoBuffer = DXTypedSingularBuffer<HLSL_ShaderMaterialInfo>::CreateAndUpload(Device, s2ws(mat.name) + L"_MaterialInfo", cmdList, frameCtx.IntermediateResources.emplace_back(), matInfo, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		material.MaterialInfoBuffer.CreatePlacedCBV(material.MaterialInfo.GetView(0));
    }

    // Loop over shapes
    for (auto& shape : shapes)
    {
		auto& indexedModel = objModel->ModelViews[shape.name];
		indexedModel.Model = &objModel->Model;
        indexedModel.Name = shape.name;
		static_assert(sizeof(HLSL_VertexData) == sizeof(tinyobj::index_t));
        indexedModel.Indices = DXTypedBuffer<HLSL_VertexData>::CreateAndUpload(Device, s2ws(shape.name), cmdList, frameCtx.IntermediateResources.emplace_back(), std::span((HLSL_VertexData const*)shape.mesh.indices.data(), shape.mesh.indices.size()), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		indexedModel.IndicesView.BufferLocation = indexedModel.Indices.Resource->GetGPUVirtualAddress();
		indexedModel.IndicesView.SizeInBytes = indexedModel.Indices.Size;
		indexedModel.IndicesView.StrideInBytes = sizeof(HLSL_VertexData);
    }

	objModel->Objects.reserve(shapes.size());

    for (size_t i = 0; i < shapes.size(); ++i)
    {
		auto& shape = shapes[i];
		auto& indexedModel = objModel->ModelViews[shape.name];

		Material* mat = nullptr;

        if (!shape.mesh.material_ids.empty())
        {
            auto matName =reader.GetMaterials()[shape.mesh.material_ids[0]].name;
			assert(objModel->Materials.find(matName) != objModel->Materials.end());
			mat = &objModel->Materials[matName];
        }
		objModel->Objects.emplace_back(&indexedModel, mat);
    }

    return objModel.get();
}

}