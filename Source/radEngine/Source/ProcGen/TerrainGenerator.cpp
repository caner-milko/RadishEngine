#include "TerrainGenerator.h"

#include "ShaderManager.h"
#include "TextureManager.h"
#include <random>
#include "Compute/Terrain/TerrainResources.hlsli"
#include "Compute/Terrain/TerrainConstantBuffers.hlsli"
#include "stb_image.h"


namespace rad::proc
{
	size_t GetIndex(size_t x, size_t y, size_t width)
	{
		return x + y * width;
	}

	template<typename T>
	struct MapVector : std::vector<T>
	{
		size_t X, Y;
		MapVector(size_t x, size_t y) : std::vector<T>(x* y), X(x), Y(y) {}
		T& operator()(size_t x, size_t y)
		{
			return this->at(GetIndex(x, y, X));
		}
	};

	static std::mt19937 generator = std::mt19937();

	//from https://medium.com/@nickobrien/diamond-square-algorithm-explanation-and-c-implementation-5efa891e486f
	float random()
	{
		std::uniform_real_distribution<float> distribution(0.f, 1.f);
		return distribution(generator);
	}

	float randomRange(float range)
	{
		return (random() * 2.f - 1.f) * range;
	}

	void diamondStep(MapVector<float>& map, int x, int y, int reach)
	{
		int count = 0;
		float avg = 0.0f;
		if (x - reach >= 0)
		{
			avg += map(x - reach, y);
			count++;
		}
		if (x + reach < map.X)
		{
			avg += map(x + reach, y);
			count++;
		}
		if (y - reach >= 0)
		{
			avg += map(x, y - reach);
			count++;
		}
		if (y + reach < map.Y)
		{
			avg += map(x, y + reach);
			count++;
		}
		avg += randomRange(reach / (float)map.X);
		avg /= count;
		map(x, y) = avg;
	}

	void squareStep(MapVector<float>& map, int x, int y, int reach)
	{
		int count = 0;
		float avg = 0.0f;
		if (x - reach >= 0 && y - reach >= 0)
		{
			avg += map(x - reach, y - reach);
			count++;
		}
		if (x - reach >= 0 && y + reach < map.Y)
		{
			avg += map(x - reach, y + reach);
			count++;
		}
		if (x + reach < map.X && y - reach >= 0)
		{
			avg += map(x + reach, y - reach);
			count++;
		}
		if (x + reach < map.X && y + reach < map.Y)
		{
			avg += map(x + reach, y + reach);
			count++;
		}
		avg += randomRange(reach / (float)map.X);
		avg /= count;
		map(x, y) = avg;
	}

	void diamondSquare(MapVector<float>& map, int size)
	{
		int half = size / 2;
		if (half < 1)
			return;
		//square steps
		for (int y = half; y < map.Y; y += size)
			for (int x = half; x < map.X; x += size)
				squareStep(map, x % map.X, y % map.Y, half);
		// diamond steps
		int col = 0;
		for (int x = 0; x < map.X; x += half)
		{
			col++;
			//If this is an odd column.
			if (col % 2 == 1)
				for (int y = half; y < map.Y; y += size)
					diamondStep(map, x % map.X, y % map.Y, half);
			else
				for (int y = 0; y < map.Y; y += size)
					diamondStep(map, x % map.X, y % map.Y, half);
		}
		diamondSquare(map, size / 2);
	}

	bool TerrainGenerator::Setup(ID3D12Device2* dev)
	{
		Device = dev;
		HeightMapToMeshPSO = PipelineState::CreateComputePipeline("HeightToMeshPipeline", Device, RAD_SHADERS_DIR L"Compute/Terrain/HeightMapToMesh.hlsl", &ShaderManager::Get().BindlessRootSignature);
		HeightMapToMaterialPSO = PipelineState::CreateComputePipeline("HeightToMaterialPipeline", Device, RAD_SHADERS_DIR L"Compute/Terrain/HeightMapToMaterial.hlsl", &ShaderManager::Get().BindlessRootSignature);
		ThermalOutfluxPSO = PipelineState::CreateComputePipeline("ThermalErosionOutflux", Device, RAD_SHADERS_DIR L"Compute/Terrain/T1ThermalOutflux.hlsl", &ShaderManager::Get().BindlessRootSignature);
		ThermalDepositPSO = PipelineState::CreateComputePipeline("ThermalErosionDeposit", Device, RAD_SHADERS_DIR L"Compute/Terrain/T2ThermalDeposit.hlsl", &ShaderManager::Get().BindlessRootSignature);
		return true;
	}

	std::vector<float> TerrainGenerator::CreateDiamondSquareHeightMap(uint32_t toPowerOfTwo, uint32_t& width)
	{
		width = (1 << toPowerOfTwo) + 1;
		MapVector<float> heightMap(width, width);
		heightMap[0] = random();
		heightMap[width - 1] = random();
		heightMap[width * (width - 1)] = random();
		heightMap[width * width - 1] = random();

		diamondSquare(heightMap, width);

		//Get rid of rightmost column and bottom row to make it a power of two
		std::vector<float> heightMapVals((width - 1) * (width - 1));
		for (int y = 0; y < width - 1; y++)
			for (int x = 0; x < width - 1; x++)
				heightMapVals[GetIndex(x, y, width - 1)] = heightMap(x, y);
		width = width - 1;

		return heightMapVals;
	}

	TerrainData TerrainGenerator::InitializeTerrain(ID3D12Device* device, uint32_t resX, uint32_t resY)
	{
		return { .MeshResX = resX, .MeshResY = resY };
	}

	void TerrainGenerator::GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, uint32_t toPowerOfTwo)
	{
		//create heightmap
		uint32_t width;
		generator = std::mt19937(time(0));
		auto heightMapVals = CreateDiamondSquareHeightMap(toPowerOfTwo, width);

		//int width, height, channels;
		//float* heightMapVals = stbi_loadf(RAD_ASSETS_DIR "heightmap.png", &width, &height, &channels, 1);

		auto& heightMap = terrain.HeightMap = DXTexture::Create(device, L"HeightMap", DXTexture::TextureCreateInfo
			{
				.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Width = uint32_t(width),
				.Height = uint32_t(width),
				.MipLevels = 1,
				.Format = DXGI_FORMAT_R32_FLOAT,
				.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			}, D3D12_RESOURCE_STATE_COPY_DEST);
		heightMap.UploadDataTyped<float>(frameCtx, cmdList, heightMapVals);
		//heightMap.UploadDataTyped<float>(frameCtx, cmdList, std::span<const float>(heightMapVals, width * width));
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		terrain.HeightMapSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		heightMap.CreatePlacedSRV(terrain.HeightMapSRV.GetView(), &srvDesc);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		terrain.HeightMapUAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		heightMap.CreatePlacedUAV(terrain.HeightMapUAV.GetView(), &uavDesc);
	}

	void TerrainGenerator::ErodeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain)
	{
		uint32_t width = terrain.HeightMap.Info.Width;
		uint32_t height = terrain.HeightMap.Info.Height;

		DXTexture pipes[2];
		const DXTexture::TextureCreateInfo createInfo = {
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Width = width,
			.Height = height,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		};

		pipes[0] = DXTexture::Create(device, L"ThermalErosionPipe0", createInfo, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pipes[1] = DXTexture::Create(device, L"ThermalErosionPipe1", createInfo, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		frameCtx.IntermediateResources.push_back(pipes[0].Resource);
		frameCtx.IntermediateResources.push_back(pipes[1].Resource);

		DescriptorAllocation pipeUAVs[2] = {
			g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1),
			g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1)
		};

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;
		pipes[0].CreatePlacedUAV(pipeUAVs[0].GetView(), &uavDesc);
		pipes[1].CreatePlacedUAV(pipeUAVs[1].GetView(), &uavDesc);

		DescriptorAllocation pipeSRVs[2] = {
			g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1),
			g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1)
		};

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		pipes[0].CreatePlacedSRV(pipeSRVs[0].GetView(), &srvDesc);
		pipes[1].CreatePlacedSRV(pipeSRVs[1].GetView(), &srvDesc);

		for (int i = 0; i < 256; i++)
		{
			TransitionVec().Add(pipes[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(pipes[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.Execute(cmdList);

			hlsl::ThermalOutfluxResources outfluxResources{
				.InHeightMapIndex = terrain.HeightMapSRV.Index,
				.OutFluxTextureIndex1 = pipeUAVs[0].Index,
				.OutFluxTextureIndex2 = pipeUAVs[1].Index,
			};
			ThermalOutfluxPSO.ExecuteCompute(cmdList, outfluxResources, width / 8, height / 8, 1);

			TransitionVec().Add(pipes[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(pipes[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Execute(cmdList);

			hlsl::ThermalDepositResources depositResources{
				.InFluxTextureIndex1 = pipeSRVs[0].Index,
				.InFluxTextureIndex2 = pipeSRVs[1].Index,
				.OutHeightMapIndex = terrain.HeightMapUAV.Index,
			};
			
			ThermalDepositPSO.ExecuteCompute(cmdList, depositResources, width / 8, height / 8, 1);
		}
	}

	void TerrainGenerator::InitializeMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain)
	{
		std::vector<uint32_t> indices((terrain.MeshResX - 1) * (terrain.MeshResY - 1) * 6);
		for (uint32_t y = 0; y < terrain.MeshResY - 1; y++)
			for (uint32_t x = 0; x < terrain.MeshResX - 1; x++)
			{
				auto vtx1 = GetIndex(x, y, terrain.MeshResX);
				auto vtx2 = GetIndex(x + 1, y, terrain.MeshResX);
				auto vtx3 = GetIndex(x, y + 1, terrain.MeshResX);
				auto vtx4 = GetIndex(x + 1, y + 1, terrain.MeshResX);
				indices[6 * GetIndex(x, y, terrain.MeshResX - 1) + 0] = vtx1;
				indices[6 * GetIndex(x, y, terrain.MeshResX - 1) + 1] = vtx3;
				indices[6 * GetIndex(x, y, terrain.MeshResX - 1) + 2] = vtx2;
				indices[6 * GetIndex(x, y, terrain.MeshResX - 1) + 3] = vtx3;
				indices[6 * GetIndex(x, y, terrain.MeshResX - 1) + 4] = vtx4;
				indices[6 * GetIndex(x, y, terrain.MeshResX - 1) + 5] = vtx2;
			}

		terrain.Model = StandaloneModel{};
		auto& model = *terrain.Model;
		model.Vertices.VerticesBuffer = DXTypedBuffer<Vertex>::Create(device, L"TerrainVtxBuffer", terrain.MeshResX * terrain.MeshResY, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		auto& vtxBufView = model.Vertices.VertexBufferView;
		vtxBufView.BufferLocation = model.Vertices.VerticesBuffer.Resource->GetGPUVirtualAddress();
		vtxBufView.SizeInBytes = model.Vertices.VerticesBuffer.Size;
		vtxBufView.StrideInBytes = sizeof(Vertex);
		terrain.VerticesUAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		model.Vertices.VerticesBuffer.CreatePlacedTypedUAV(terrain.VerticesUAV.GetView());
		model.Indices = DXTypedBuffer<uint32_t>::CreateAndUpload(device, L"TerrainIdxBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), indices);
		auto& idxBufView = model.IndexBufferView;
		idxBufView.BufferLocation = model.Indices.Resource->GetGPUVirtualAddress();
		idxBufView.SizeInBytes = model.Indices.Size;
		idxBufView.Format = DXGI_FORMAT_R32_UINT;
	}

	void TerrainGenerator::InitializeMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain)
	{

		terrain.Material = Material{};
		auto& material = *terrain.Material;
		terrain.AlbedoTex = DXTexture::Create(device, L"TerrainAlbedo", DXTexture::TextureCreateInfo
			{
				.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Width = 1024,
				.Height = 1024,
				.MipLevels = 0,
				.Format = DXGI_FORMAT_R16G16B16A16_UNORM,
				.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			});
		D3D12_SHADER_RESOURCE_VIEW_DESC albedoSRVDesc = {};
		albedoSRVDesc.Format = terrain.AlbedoTex.Info.Format;
		albedoSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		albedoSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		albedoSRVDesc.Texture2D.MipLevels = -1;
		terrain.AlbedoTexSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		terrain.AlbedoTex.CreatePlacedSRV(terrain.AlbedoTexSRV.GetView(), &albedoSRVDesc);
		D3D12_UNORDERED_ACCESS_VIEW_DESC albedoUAVDesc = {};
		albedoUAVDesc.Format = terrain.AlbedoTex.Info.Format;
		albedoUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		terrain.AlbedoTexUAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		terrain.AlbedoTex.CreatePlacedUAV(terrain.AlbedoTexUAV.GetView(), &albedoUAVDesc);

		terrain.NormalMap = DXTexture::Create(device, L"TerrainNormal", terrain.AlbedoTex.Info);
		D3D12_SHADER_RESOURCE_VIEW_DESC normalSRVDesc = {};
		normalSRVDesc.Format = terrain.NormalMap.Info.Format;
		normalSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		normalSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		normalSRVDesc.Texture2D.MipLevels = -1;
		terrain.NormalMapSRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		terrain.NormalMap.CreatePlacedSRV(terrain.NormalMapSRV.GetView(), &normalSRVDesc);
		D3D12_UNORDERED_ACCESS_VIEW_DESC normalUAVDesc = {};
		normalUAVDesc.Format = terrain.NormalMap.Info.Format;
		normalUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		terrain.NormalMapUAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		terrain.NormalMap.CreatePlacedUAV(terrain.NormalMapUAV.GetView(), &normalUAVDesc);

		material.DiffuseTextureSRV = terrain.AlbedoTexSRV;
		material.NormalMapTextureSRV = terrain.NormalMapSRV;
		hlsl::MaterialBuffer materialBuffer{
			.DiffuseTextureIndex = material.DiffuseTextureSRV->Index,
			.NormalMapTextureIndex = material.NormalMapTextureSRV->Index,
		};
		material.MaterialInfoBuffer = DXTypedSingularBuffer<rad::hlsl::MaterialBuffer>::CreateAndUpload(device, L"TerrainMaterialBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), materialBuffer);
		material.MaterialInfo = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		material.MaterialInfoBuffer.CreatePlacedCBV(material.MaterialInfo.GetView());

		TransitionVec().Add(terrain.AlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Add(terrain.NormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Execute(cmdList);
	}

	void TerrainGenerator::GenerateMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain)
	{
		//std::vector<Vertex> vertices(terrain.MeshResX * terrain.MeshResY);
		//for (uint32_t y = 0; y < width; y++)
		//	for (uint32_t x = 0; x < width; x++)
		//		vertices[GetIndex(x, y, width)] = ToVertex(terrain, heightMapVals, x, y);
		TransitionVec().Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.Model->Vertices.VerticesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(cmdList);
		
		hlsl::HeightToMeshResources resources{
			.HeightMapTextureIndex = terrain.HeightMapSRV.Index,
			.VertexBufferIndex = terrain.VerticesUAV.Index,
			.MeshResX = terrain.MeshResX,
			.MeshResY = terrain.MeshResY
		};
		HeightMapToMeshPSO.ExecuteCompute(cmdList, resources, terrain.MeshResX / 8, terrain.MeshResY / 8, 1);

		TransitionVec().Add(terrain.Model->Vertices.VerticesBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER).Execute(cmdList);
	}

	void TerrainGenerator::GenerateMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain)
	{
		TransitionVec().Add(terrain.AlbedoTex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.NormalMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Execute(cmdList);

		hlsl::HeightToMaterialResources resources{
			.HeightMapTextureIndex = terrain.HeightMapSRV.Index,
			.AlbedoTextureIndex = terrain.AlbedoTexUAV.Index,
			.NormalMapTextureIndex = terrain.NormalMapUAV.Index,
		};

		HeightMapToMaterialPSO.ExecuteCompute(cmdList, resources, terrain.HeightMap.Info.Width / 8, terrain.HeightMap.Info.Height / 8, 1);

		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.AlbedoTex);
		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.NormalMap);
		TransitionVec().Add(terrain.AlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Add(terrain.NormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Execute(cmdList);
	}
}