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

		HydrolicAddWaterPSO = PipelineState::CreateComputePipeline("HydrolicAddWater", Device, RAD_SHADERS_DIR L"Compute/Terrain/H1AddWater.hlsl", &ShaderManager::Get().BindlessRootSignature);
		HydrolicCalculateOutfluxPSO = PipelineState::CreateComputePipeline("HydrolicCalculateOutflux", Device, RAD_SHADERS_DIR L"Compute/Terrain/H2CalculateOutflux.hlsl", &ShaderManager::Get().BindlessRootSignature);
		HydrolicUpdateWaterVelocityPSO = PipelineState::CreateComputePipeline("HydrolicUpdateWaterVelocity", Device, RAD_SHADERS_DIR L"Compute/Terrain/H3UpdateWaterVelocity.hlsl", &ShaderManager::Get().BindlessRootSignature);
		HydrolicErosionAndDepositionPSO = PipelineState::CreateComputePipeline("HydrolicErosionAndDeposition", Device, RAD_SHADERS_DIR L"Compute/Terrain/H4ErosionAndDeposition.hlsl", &ShaderManager::Get().BindlessRootSignature);
		HydrolicSedimentTransportationAndEvaporationPSO = PipelineState::CreateComputePipeline("HydrolicSedimentTransportationAndEvaporation", Device, RAD_SHADERS_DIR L"Compute/Terrain/H5SedimentTransportationAndEvaporation.hlsl", &ShaderManager::Get().BindlessRootSignature);

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
				heightMapVals[GetIndex(x, y, width - 1)] = heightMap(x, y);
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
		terrain.HeightMap = DXTexture::Create(device, L"HeightMap", baseTextureInfo);
		terrain.WaterHeightMap = DXTexture::Create(device, L"WaterHeightMap", baseTextureInfo);
		terrain.SedimentMap = DXTexture::Create(device, L"SedimentMap", baseTextureInfo);
		terrain.TempHeightMap = DXTexture::Create(device, L"TempHeightMap", baseTextureInfo);
		terrain.TempSedimentMap = DXTexture::Create(device, L"TempSedimentMap", baseTextureInfo);
		terrain.SoftnessMap = DXTexture::Create(device, L"HardnessMap", baseTextureInfo);

		baseTextureInfo.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		terrain.WaterOutflux = DXTexture::Create(device, L"WaterOutflux", baseTextureInfo);
		terrain.ThermalPipe1 = DXTexture::Create(device, L"ThermalPipe1", baseTextureInfo);
		terrain.ThermalPipe2 = DXTexture::Create(device, L"ThermalPipe2", baseTextureInfo);

		baseTextureInfo.Format = DXGI_FORMAT_R32G32_FLOAT;
		terrain.VelocityMap = DXTexture::Create(device, L"VelocityMap", baseTextureInfo);

		InitializeMesh(device, frameCtx, cmdList, terrain);
		InitializeMaterial(device, frameCtx, cmdList, terrain);
		return terrain;
	}

	void TerrainGenerator::GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters, bool generateMeshAndMaterial)
	{
		//create heightmap
		constexpr auto scaleHeightMaps = [](float* data, size_t size, float min, float max)
			{
				float valsMin = FLT_MAX, valsMax = FLT_MIN;
				for (size_t i = 0; i < size; i++)
				{
					valsMin = std::min(valsMin, data[i]);
					valsMax = std::max(valsMax, data[i]);
				}
				float oneOverRange = 1.0 / (valsMax - valsMin);
				for (size_t i = 0; i < size; i++)
				{

					data[i] = std::pow((data[i] - valsMin) * oneOverRange, 2.0f) * (max - min) + min;
				}
			};
		if (parameters.BaseFromFile)
		{
			int width, height, channels;
			float* heightMapVals = stbi_loadf(RAD_ASSETS_DIR "heightmap.png", &width, &height, &channels, 1);
			scaleHeightMaps(heightMapVals, width * height, parameters.MinHeight, parameters.MaxHeight);
			terrain.HeightMap.UploadDataTyped<float>(frameCtx, cmdList, std::span<const float>(heightMapVals, width * width));
		}
		else
		{
			if(!parameters.Random)
				generator = std::mt19937(parameters.Seed);
			else
				generator = std::mt19937(time(0));
			auto heightMapVals = CreateDiamondSquareHeightMap(terrain.HeightMap.Info.Width, parameters.InitialRoughness);
			scaleHeightMaps(heightMapVals.data(), heightMapVals.size(), parameters.MinHeight, parameters.MaxHeight);
			terrain.HeightMap.UploadDataTyped<float>(frameCtx, cmdList, heightMapVals);
		}

		// Clear water/sediment/outflux/hardness maps
		TransitionVec().Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.SedimentMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.WaterOutflux, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.SoftnessMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(cmdList);
		float clearValue[4] = { 0.f, 0.f, 0.f, 0.f };
		auto cpuUAV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = terrain.WaterHeightMap.Info.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;
		terrain.WaterHeightMap.CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
		cmdList->ClearUnorderedAccessViewFloat(terrain.WaterHeightMap.UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(), terrain.WaterHeightMap.Resource.Get(), clearValue, 0, nullptr);
		uavDesc.Format = terrain.SedimentMap.Info.Format;
		terrain.SedimentMap.CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
		cmdList->ClearUnorderedAccessViewFloat(terrain.SedimentMap.UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(), terrain.SedimentMap.Resource.Get(), clearValue, 0, nullptr);
		uavDesc.Format = terrain.WaterOutflux.Info.Format;
		terrain.WaterOutflux.CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
		cmdList->ClearUnorderedAccessViewFloat(terrain.WaterOutflux.UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(), terrain.WaterOutflux.Resource.Get(), clearValue, 0, nullptr);
		uavDesc.Format = terrain.SoftnessMap.Info.Format;
		float hardnessClearValue[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
		terrain.SoftnessMap.CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
		cmdList->ClearUnorderedAccessViewFloat(terrain.SoftnessMap.UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(), terrain.SoftnessMap.Resource.Get(), hardnessClearValue, 0, nullptr);
		g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StaticPage->Top--;
		terrain.IterationCount = 0;
		if (generateMeshAndMaterial)
		{
			GenerateMesh(device, frameCtx, cmdList, terrain, parameters);
			GenerateMaterial(device, frameCtx, cmdList, terrain, parameters);
		}
	}

	void TerrainGenerator::ErodeTerrain(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters, bool generateMeshAndMaterial)
	{

		uint32_t width = terrain.HeightMap.Info.Width;
		uint32_t height = terrain.HeightMap.Info.Height;

		float pipeLength = parameters.TotalLength / width;
		float crossSection = parameters.PipeCrossSection * pipeLength * pipeLength;

		hlsl::ThermalOutfluxResources outfluxResources{
			.InHeightMapIndex = terrain.HeightMap.SRV.Index,
			.InHardnessMapIndex = terrain.SoftnessMap.SRV.Index,
			.OutFluxTextureIndex1 = terrain.ThermalPipe1.UAV.Index,
			.OutFluxTextureIndex2 = terrain.ThermalPipe2.UAV.Index,
			.ThermalErosionRate = parameters.ThermalErosionRate,
			.PipeLength = pipeLength,
			.SoftnessTalusCoefficient = parameters.SoftnessTalusCoefficient,
			.MinTalusCoefficient = parameters.MinTalusCoefficient,
		};

		hlsl::ThermalDepositResources depositResources{
			.InFluxTextureIndex1 = terrain.ThermalPipe1.SRV.Index,
			.InFluxTextureIndex2 = terrain.ThermalPipe2.SRV.Index,
			.OutHeightMapIndex = terrain.HeightMap.UAV.Index,
		};

		for (int i = 0; i < parameters.Iterations; i++)
		{
			hlsl::HydrolicAddWaterResources addWaterResources{
				.WaterMapIndex = terrain.WaterHeightMap.UAV.Index,
				.RainRate = parameters.RainRate,
				.Iteration = terrain.IterationCount,
			};
			TransitionVec().Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(cmdList);
			HydrolicAddWaterPSO.ExecuteCompute(cmdList, addWaterResources, width / 8, height / 8, 1);
			
			hlsl::HydrolicCalculateOutfluxResources calculateOutfluxResources{
				.InHeightMapIndex = terrain.HeightMap.SRV.Index,
				.InWaterMapIndex = terrain.WaterHeightMap.SRV.Index,
				.OutFluxTextureIndex = terrain.WaterOutflux.UAV.Index,
				.PipeCrossSection = crossSection,
				.PipeLength = pipeLength,
			};
			TransitionVec().Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.WaterOutflux, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(cmdList);
			HydrolicCalculateOutfluxPSO.ExecuteCompute(cmdList, calculateOutfluxResources, width / 8, height / 8, 1);
			
			hlsl::HydrolicUpdateWaterVelocityResources updateWaterVelocityResources{
				.InFluxTextureIndex = terrain.WaterOutflux.SRV.Index,
				.OutWaterMapIndex = terrain.WaterHeightMap.UAV.Index,
				.OutVelocityMapIndex = terrain.VelocityMap.UAV.Index,
				.PipeLength = pipeLength,
			};
			TransitionVec().Add(terrain.WaterOutflux, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.VelocityMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(cmdList);
			HydrolicUpdateWaterVelocityPSO.ExecuteCompute(cmdList, updateWaterVelocityResources, width / 8, height / 8, 1);
			
			hlsl::HydrolicErosionAndDepositionResources erosionAndDepositionResources{
				.InVelocityMapIndex = terrain.VelocityMap.SRV.Index,
				.InOldHeightMapIndex = terrain.HeightMap.SRV.Index,
				.InOutSoftnessMapIndex = terrain.SoftnessMap.UAV.Index,
				.OutHeightMapIndex = terrain.TempHeightMap.UAV.Index,
				.OutWaterMapIndex = terrain.WaterHeightMap.UAV.Index,
				.OutSedimentMapIndex = terrain.SedimentMap.UAV.Index,
				.PipeLength = pipeLength,
				.SedimentCapacity = parameters.SedimentCapacity,
				.SoilSuspensionRate = parameters.SoilSuspensionRate,
				.SedimentDepositionRate = parameters.SedimentDepositionRate,
				.SoilHardeningRate = parameters.SoilHardeningRate,
				.SoilSofteningRate = parameters.SoilSofteningRate,
				.MinimumSoftness = parameters.MinimumSoilSoftness,
				.MaximalErosionDepth = parameters.MaximalErosionDepth,
			};
			TransitionVec().Add(terrain.VelocityMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.TempHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.SedimentMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.SoftnessMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).
				Execute(cmdList);
			HydrolicErosionAndDepositionPSO.ExecuteCompute(cmdList, erosionAndDepositionResources, width / 8, height / 8, 1);
			// Copy temp height map to height map
			TransitionVec().Add(terrain.TempHeightMap, D3D12_RESOURCE_STATE_COPY_SOURCE).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_COPY_DEST).Execute(cmdList);
			cmdList->CopyResource(terrain.HeightMap.Resource.Get(), terrain.TempHeightMap.Resource.Get());


			hlsl::HydrolicSedimentTransportationAndEvaporationResources sedimentTransportationAndEvaporationResources{
				.InVelocityMapIndex = terrain.VelocityMap.SRV.Index,
				.InOldSedimentMapIndex = terrain.SedimentMap.SRV.Index,
				.OutSedimentMapIndex = terrain.TempSedimentMap.UAV.Index,
				.PipeLength = pipeLength,
				.InOutWaterMapIndex = terrain.WaterHeightMap.UAV.Index,
				.EvaporationRate = parameters.EvaporationRate,
			};
			TransitionVec().Add(terrain.VelocityMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.SedimentMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.TempSedimentMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(cmdList);
			HydrolicSedimentTransportationAndEvaporationPSO.ExecuteCompute(cmdList, sedimentTransportationAndEvaporationResources, width / 8, height / 8, 1);
			// Copy temp sediment map to sediment map
			TransitionVec().Add(terrain.TempSedimentMap, D3D12_RESOURCE_STATE_COPY_SOURCE).Add(terrain.SedimentMap, D3D12_RESOURCE_STATE_COPY_DEST).Execute(cmdList);
			cmdList->CopyResource(terrain.SedimentMap.Resource.Get(), terrain.TempSedimentMap.Resource.Get());

			TransitionVec().Add(terrain.ThermalPipe1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.ThermalPipe2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.SoftnessMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Execute(cmdList);
			ThermalOutfluxPSO.ExecuteCompute(cmdList, outfluxResources, width / 8, height / 8, 1);

			TransitionVec().Add(terrain.ThermalPipe1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE).Add(terrain.ThermalPipe2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Add(terrain.HeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Execute(cmdList);
			ThermalDepositPSO.ExecuteCompute(cmdList, depositResources, width / 8, height / 8, 1);

			terrain.IterationCount++;
		}

		if (generateMeshAndMaterial)
		{
			GenerateMesh(device, frameCtx, cmdList, terrain, parameters);
			GenerateMaterial(device, frameCtx, cmdList, terrain, parameters);
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

		terrain.TerrainModel = StandaloneModel{};
		auto& model = *terrain.TerrainModel;
		model.Vertices.VerticesBuffer = DXTypedBuffer<Vertex>::Create(device, L"TerrainVtxBuffer", terrain.MeshResX * terrain.MeshResY, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		auto& vtxBufView = model.Vertices.VertexBufferView;
		vtxBufView.BufferLocation = model.Vertices.VerticesBuffer.Resource->GetGPUVirtualAddress();
		vtxBufView.SizeInBytes = model.Vertices.VerticesBuffer.Size;
		vtxBufView.StrideInBytes = sizeof(Vertex);
		terrain.TerrainVerticesUAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		model.Vertices.VerticesBuffer.CreatePlacedTypedUAV(terrain.TerrainVerticesUAV.GetView());
		model.Indices = DXTypedBuffer<uint32_t>::CreateAndUpload(device, L"TerrainIdxBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), indices);
		auto& idxBufView = model.IndexBufferView;
		idxBufView.BufferLocation = model.Indices.Resource->GetGPUVirtualAddress();
		idxBufView.SizeInBytes = model.Indices.Size;
		idxBufView.Format = DXGI_FORMAT_R32_UINT;

		terrain.WaterModel = StandaloneModel{};
		auto& waterModel = *terrain.WaterModel;
		waterModel.Vertices.VerticesBuffer = DXTypedBuffer<Vertex>::Create(device, L"WaterVtxBuffer", terrain.MeshResX * terrain.MeshResY, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		auto& waterVtxBufView = waterModel.Vertices.VertexBufferView;
		waterVtxBufView.BufferLocation = waterModel.Vertices.VerticesBuffer.Resource->GetGPUVirtualAddress();
		waterVtxBufView.SizeInBytes = waterModel.Vertices.VerticesBuffer.Size;
		waterVtxBufView.StrideInBytes = sizeof(Vertex);
		terrain.WaterVerticesUAV = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		waterModel.Vertices.VerticesBuffer.CreatePlacedTypedUAV(terrain.WaterVerticesUAV.GetView());
		waterModel.Indices = DXTypedBuffer<uint32_t>::CreateAndUpload(device, L"WaterIdxBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), indices);
		auto& waterIdxBufView = waterModel.IndexBufferView;
		waterIdxBufView.BufferLocation = waterModel.Indices.Resource->GetGPUVirtualAddress();
		waterIdxBufView.SizeInBytes = waterModel.Indices.Size;
		waterIdxBufView.Format = DXGI_FORMAT_R32_UINT;
	}

	void TerrainGenerator::InitializeMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain)
	{
		terrain.TerrainMaterial = Material{};
		auto& material = *terrain.TerrainMaterial;
		auto materialTexInfo = DXTexture::TextureCreateInfo
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Width = 1024,
			.Height = 1024,
			.MipLevels = 0,
			.Format = DXGI_FORMAT_R16G16B16A16_UNORM,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		};
		terrain.TerrainAlbedoTex = RWTexture(DXTexture::Create(device, L"TerrainAlbedo", materialTexInfo), -1);

		terrain.TerrainNormalMap = RWTexture(DXTexture::Create(device, L"TerrainNormal", materialTexInfo), -1);
		
		material.DiffuseTextureSRV = terrain.TerrainAlbedoTex.SRV;
		material.NormalMapTextureSRV = terrain.TerrainNormalMap.SRV;
		hlsl::MaterialBuffer materialBuffer{
			.DiffuseTextureIndex = material.DiffuseTextureSRV->Index,
			.NormalMapTextureIndex = material.NormalMapTextureSRV->Index,
		};
		material.MaterialInfoBuffer = DXTypedSingularBuffer<rad::hlsl::MaterialBuffer>::CreateAndUpload(device, L"TerrainMaterialBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), materialBuffer);
		material.MaterialInfo = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		material.MaterialInfoBuffer.CreatePlacedCBV(material.MaterialInfo.GetView());
		TransitionVec().Add(terrain.TerrainAlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Add(terrain.TerrainNormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Execute(cmdList);

		terrain.WaterMaterial = Material{};
		auto& waterMaterial = *terrain.WaterMaterial;
		terrain.WaterAlbedoTex = RWTexture(DXTexture::Create(device, L"WaterAlbedo", materialTexInfo), -1);
		terrain.WaterNormalMap = RWTexture(DXTexture::Create(device, L"WaterNormal", materialTexInfo), -1);
		waterMaterial.DiffuseTextureSRV = terrain.WaterAlbedoTex.SRV;
		waterMaterial.NormalMapTextureSRV = terrain.WaterNormalMap.SRV;
		hlsl::MaterialBuffer waterMaterialBuffer{
			.DiffuseTextureIndex = terrain.WaterAlbedoTex.SRV.Index,
			.NormalMapTextureIndex = waterMaterial.NormalMapTextureSRV->Index,
		};
		waterMaterial.MaterialInfoBuffer = DXTypedSingularBuffer<rad::hlsl::MaterialBuffer>::CreateAndUpload(device, L"WaterMaterialBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), waterMaterialBuffer);
		waterMaterial.MaterialInfo = g_GPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		waterMaterial.MaterialInfoBuffer.CreatePlacedCBV(waterMaterial.MaterialInfo.GetView());
		TransitionVec()
			.Add(terrain.WaterAlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			.Add(terrain.WaterNormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE).Execute(cmdList);
	}

	void TerrainGenerator::GenerateMesh(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters)
	{
		//std::vector<Vertex> vertices(terrain.MeshResX * terrain.MeshResY);
		//for (uint32_t y = 0; y < width; y++)
		//	for (uint32_t x = 0; x < width; x++)
		//		vertices[GetIndex(x, y, width)] = ToVertex(terrain, heightMapVals, x, y);
		TransitionVec()
			.Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.Add(terrain.TerrainModel->Vertices.VerticesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Add(terrain.WaterModel->Vertices.VerticesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Execute(cmdList);
		
		hlsl::HeightToMeshResources resources{
			.HeightMapTextureIndex = terrain.HeightMap.SRV.Index,
			.WaterHeightMapTextureIndex = terrain.WaterHeightMap.SRV.Index,
			.TerrainVertexBufferIndex = terrain.TerrainVerticesUAV.Index,
			.WaterVertexBufferIndex = terrain.WaterVerticesUAV.Index,
			.MeshResX = terrain.MeshResX,
			.MeshResY = terrain.MeshResY,
			.CellSize = parameters.TotalLength / terrain.HeightMap.Info.Width,
		};
		HeightMapToMeshPSO.ExecuteCompute(cmdList, resources, terrain.MeshResX / 8, terrain.MeshResY / 8, 1);

		TransitionVec()
			.Add(terrain.TerrainModel->Vertices.VerticesBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
			.Add(terrain.WaterModel->Vertices.VerticesBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
			.Execute(cmdList);
	}

	void TerrainGenerator::GenerateMaterial(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, TerrainData& terrain, ErosionParameters const& parameters)
	{
		TransitionVec()
			.Add(terrain.TerrainAlbedoTex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Add(terrain.TerrainNormalMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Add(terrain.WaterAlbedoTex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Add(terrain.WaterNormalMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Add(terrain.HeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.Add(terrain.WaterHeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.Add(terrain.SedimentMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.Execute(cmdList);

		float cellSize = parameters.TotalLength / terrain.HeightMap.Info.Width;

		hlsl::HeightToMaterialResources resources{
			.HeightMapTextureIndex = terrain.HeightMap.SRV.Index,
			.WaterHeightMapTextureIndex = terrain.WaterHeightMap.SRV.Index,
			.SedimentMapTextureIndex = terrain.SedimentMap.SRV.Index,
			.TerrainAlbedoTextureIndex = terrain.TerrainAlbedoTex.UAV.Index,
			.TerrainNormalMapTextureIndex = terrain.TerrainNormalMap.UAV.Index,
			.WaterAlbedoTextureIndex = terrain.WaterAlbedoTex.UAV.Index,
			.WaterNormalMapTextureIndex = terrain.WaterNormalMap.UAV.Index,
			.CellSize = cellSize,
		};

		HeightMapToMaterialPSO.ExecuteCompute(cmdList, resources, terrain.TerrainAlbedoTex.Info.Width / 8, terrain.TerrainAlbedoTex.Info.Height / 8, 1);

		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.TerrainAlbedoTex);
		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.TerrainNormalMap);
		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.WaterAlbedoTex);
		TextureManager::Get().GenerateMips(frameCtx, cmdList, terrain.WaterNormalMap);
		TransitionVec()
			.Add(terrain.TerrainAlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			.Add(terrain.TerrainNormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			.Add(terrain.WaterAlbedoTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			.Add(terrain.WaterNormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			.Execute(cmdList);
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