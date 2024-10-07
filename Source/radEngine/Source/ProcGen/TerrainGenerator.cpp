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

	void diamondStep(MapVector<float>& map, int x, int y, int reach, float roughness)
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
		avg /= (float)count;
		avg += randomRange(reach / (float)map.X) * roughness;
		map(x, y) = avg;
	}

	void squareStep(MapVector<float>& map, int x, int y, int reach, float roughness)
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
		avg /= (float)count;
		avg += randomRange(reach / (float)(map.X - 1)) * roughness;
		map(x, y) = avg;
	}

	void diamondSquare(MapVector<float>& map, int size, float roughness)
	{
		int half = size / 2;
		if (half < 1)
			return;
		//square steps
		for (int y = half; y < map.Y; y += size)
			for (int x = half; x < map.X; x += size)
				squareStep(map, x % map.X, y % map.Y, half, roughness);
		// diamond steps
		int col = 0;
		for (int x = 0; x < map.X; x += half)
		{
			col++;
			//If this is an odd column.
			if (col % 2 == 1)
				for (int y = half; y < map.Y; y += size)
					diamondStep(map, x % map.X, y % map.Y, half, roughness);
			else
				for (int y = 0; y < map.Y; y += size)
					diamondStep(map, x % map.X, y % map.Y, half, roughness);
		}
		diamondSquare(map, size / 2, roughness);
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

	std::vector<float> TerrainGenerator::CreateDiamondSquareHeightMap(uint32_t width, float roughness)
	{
		// Make sure width is a power of two
		assert(std::log2(width) == std::floor(std::log2(width)));

		width = width + 1;
		MapVector<float> heightMap(width, width);
		heightMap[0] = random();
		heightMap[width - 1] = random();
		heightMap[width * (width - 1)] = random();
		heightMap[width * width - 1] = random();

		diamondSquare(heightMap, width - 1, roughness);

		//Get rid of rightmost column and bottom row to make it a power of two
		float min = FLT_MAX, max = FLT_MIN;
		std::vector<float> heightMapVals((width - 1) * (width - 1));
		for (int y = 0; y < width - 1; y++)
			for (int x = 0; x < width - 1; x++)
			{
				heightMapVals[GetIndex(x, y, width - 1)] = heightMap(x, y);
				min = std::min(min, heightMap(x, y));
				max = std::max(max, heightMap(x, y));
			}
		float oneOverRange = 1.f / (max - min);
		for (auto& val : heightMapVals)
			val = (val - min) * oneOverRange;
		width = width - 1;

		return heightMapVals;
	}

	TerrainData TerrainGenerator::InitializeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, uint32_t resX, uint32_t resY, uint32_t heightMapWidth)
	{
		generator = std::mt19937(time(0));
		TerrainData terrain{ .MeshResX = resX, .MeshResY = resY };
		DXTexture::TextureCreateInfo baseTextureInfo =
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Width = uint32_t(heightMapWidth),
			.Height = uint32_t(heightMapWidth),
			.MipLevels = 1,
			.Format = DXGI_FORMAT_R32_FLOAT,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		};
		terrain.HeightMap = DXTexture::Create(device, L"HeightMap", baseTextureInfo, D3D12_RESOURCE_STATE_COPY_DEST);
		terrain.WaterHeightMap = DXTexture::Create(device, L"WaterHeightMap", baseTextureInfo);
		terrain.SedimentMap = DXTexture::Create(device, L"SedimentMap", baseTextureInfo);
		auto vectorTexInfo = baseTextureInfo;
		vectorTexInfo.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		terrain.WaterOutflux = DXTexture::Create(device, L"WaterOutflux", baseTextureInfo);
		terrain.VelocityMap = DXTexture::Create(device, L"VelocityMap", vectorTexInfo);
		terrain.TempHeightMap = DXTexture::Create(device, L"TempHeightMap", baseTextureInfo);
		terrain.TempSedimentMap = DXTexture::Create(device, L"TempSedimentMap", baseTextureInfo);

		InitializeMesh(device, frameCtx, cmdList, terrain);
		InitializeMaterial(device, frameCtx, cmdList, terrain);
		return terrain;
	}

	void TerrainGenerator::GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, float roughness, bool generateMeshAndMaterial)
	{
		//create heightmap
		auto heightMapVals = CreateDiamondSquareHeightMap(terrain.HeightMap.Info.Width, roughness);

		//int width, height, channels;
		//float* heightMapVals = stbi_loadf(RAD_ASSETS_DIR "heightmap.png", &width, &height, &channels, 1);
		terrain.HeightMap.UploadDataTyped<float>(frameCtx, cmdList, heightMapVals);
		//heightMap.UploadDataTyped<float>(frameCtx, cmdList, std::span<const float>(heightMapVals, width * width));

		if (generateMeshAndMaterial)
		{
			GenerateMesh(device, frameCtx, cmdList, terrain);
			GenerateMaterial(device, frameCtx, cmdList, terrain);
		}
	}

	void TerrainGenerator::ErodeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters parameters, bool generateMeshAndMaterial)
	{
		uint32_t width = terrain.HeightMap.Info.Width;
		uint32_t height = terrain.HeightMap.Info.Height;

		RWTexture pipes[2];
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

		hlsl::ThermalOutfluxResources outfluxResources{
			.InHeightMapIndex = terrain.HeightMap.SRV.Index,
			.OutFluxTextureIndex1 = pipes[0].UAV.Index,
			.OutFluxTextureIndex2 = pipes[1].UAV.Index,
			.CellSize = parameters.CellSize,
			.HeightToWidthRatio = parameters.HeightToWidthRatio,
			.TalusAnglePrecomputed = std::tanf(DirectX::XM_PI * parameters.TalusAngleDegrees / 180.f),
		};

		hlsl::ThermalDepositResources depositResources{
				.InFluxTextureIndex1 = pipes[0].SRV.Index,
				.InFluxTextureIndex2 = pipes[1].SRV.Index,
				.OutHeightMapIndex = terrain.HeightMap.UAV.Index,
		};

		for (int i = 0; i < parameters.Iterations; i++)
		{
			TransitionVec().Add(pipes[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(pipes[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.Execute(cmdList);

			ThermalOutfluxPSO.ExecuteCompute(cmdList, outfluxResources, width / 8, height / 8, 1);

			TransitionVec().Add(pipes[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(pipes[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Execute(cmdList);

			
			
			ThermalDepositPSO.ExecuteCompute(cmdList, depositResources, width / 8, height / 8, 1);
		}

		if (generateMeshAndMaterial)
		{
			GenerateMesh(device, frameCtx, cmdList, terrain);
			GenerateMaterial(device, frameCtx, cmdList, terrain);
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
		auto materialTexInfo = DXTexture::TextureCreateInfo
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Width = 1024,
			.Height = 1024,
			.MipLevels = 0,
			.Format = DXGI_FORMAT_R16G16B16A16_UNORM,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		};
		terrain.AlbedoTex = RWTexture(DXTexture::Create(device, L"TerrainAlbedo", materialTexInfo), -1);

		terrain.NormalMap = RWTexture(DXTexture::Create(device, L"TerrainNormal", materialTexInfo), -1);
		
		material.DiffuseTextureSRV = terrain.AlbedoTex.SRV;
		material.NormalMapTextureSRV = terrain.NormalMap.SRV;
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
			.HeightMapTextureIndex = terrain.HeightMap.SRV.Index,
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
			.HeightMapTextureIndex = terrain.HeightMap.SRV.Index,
			.AlbedoTextureIndex = terrain.AlbedoTex.UAV.Index,
			.NormalMapTextureIndex = terrain.NormalMap.UAV.Index,
		};

		HeightMapToMaterialPSO.ExecuteCompute(cmdList, resources, terrain.AlbedoTex.Info.Width / 8, terrain.AlbedoTex.Info.Height / 8, 1);

		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.AlbedoTex);
		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.NormalMap);
		TransitionVec().Add(terrain.AlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Add(terrain.NormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Execute(cmdList);
	}
	RWTexture::RWTexture(DXTexture texture, int srvMipLevels) : DXTexture(std::move(texture))
	{
		UAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		SRV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = Info.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;
		CreatePlacedUAV(UAV.GetView(), &uavDesc);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = Info.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = srvMipLevels;
		CreatePlacedSRV(SRV.GetView(), &srvDesc);
	}
}