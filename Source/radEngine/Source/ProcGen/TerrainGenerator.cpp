#include "TerrainGenerator.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/TextureManager.h"
#include <random>
#include "Compute/Terrain/TerrainResources.hlsli"
#include "Compute/Terrain/TerrainConstantBuffers.hlsli"
#include "Systems.h"
#include "stb_image.h"

namespace rad::proc
{
size_t GetIndex(size_t x, size_t y, size_t width)
{
	return x + y * width;
}

template <typename T> struct MapVector : std::vector<T>
{
	size_t X, Y;
	MapVector(size_t x, size_t y) : std::vector<T>(x * y), X(x), Y(y) {}
	T& operator()(size_t x, size_t y)
	{
		return this->at(GetIndex(x, y, X));
	}
};

static std::mt19937 generator = std::mt19937();

// from https://medium.com/@nickobrien/diamond-square-algorithm-explanation-and-c-implementation-5efa891e486f
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
	// square steps
	for (int y = half; y < map.Y; y += size)
		for (int x = half; x < map.X; x += size)
			squareStep(map, x % map.X, y % map.Y, half, roughness);
	// diamond steps
	int col = 0;
	for (int x = 0; x < map.X; x += half)
	{
		col++;
		// If this is an odd column.
		if (col % 2 == 1)
			for (int y = half; y < map.Y; y += size)
				diamondStep(map, x % map.X, y % map.Y, half, roughness);
		else
			for (int y = 0; y < map.Y; y += size)
				diamondStep(map, x % map.X, y % map.Y, half, roughness);
	}
	diamondSquare(map, size / 2, roughness);
}

bool TerrainErosionSystem::Setup()
{
	HeightMapToTerrainMaterialPSO = PipelineState::CreateBindlessComputePipeline(
		"HeightToTerrainMaterialPipeline", Renderer,
		RAD_SHADERS_DIR L"Compute/Terrain/HeightMapToTerrainMaterial.hlsl");
	HeightMapToWaterMaterialPSO = PipelineState::CreateBindlessComputePipeline(
		"HeightToWaterMaterialPipeline", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/HeightMapToWaterMaterial.hlsl");

	ThermalOutfluxPSO = PipelineState::CreateBindlessComputePipeline(
		"ThermalErosionOutflux", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/T1ThermalOutflux.hlsl");
	ThermalDepositPSO = PipelineState::CreateBindlessComputePipeline(
		"ThermalErosionDeposit", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/T2ThermalDeposit.hlsl");

	HydrolicAddWaterPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicAddWater", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/H1AddWater.hlsl");
	HydrolicCalculateOutfluxPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicCalculateOutflux", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/H2CalculateOutflux.hlsl");
	HydrolicUpdateWaterVelocityPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicUpdateWaterVelocity", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/H3UpdateWaterVelocity.hlsl");
	HydrolicErosionAndDepositionPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicErosionAndDeposition", Renderer, RAD_SHADERS_DIR L"Compute/Terrain/H4ErosionAndDeposition.hlsl");
	HydrolicSedimentTransportationAndEvaporationPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicSedimentTransportationAndEvaporation", Renderer,
		RAD_SHADERS_DIR L"Compute/Terrain/H5SedimentTransportationAndEvaporation.hlsl");

	{
		struct TerraionRenderPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} deferredPSStream;

		deferredPSStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		auto [vertexShader, pixelShader] = Renderer.ShaderManager->CompileBindlessGraphicsShader(
			L"RenderTerrain", RAD_SHADERS_DIR L"Compute/Terrain/RenderTerrain.hlsl");

		deferredPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		deferredPSStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		deferredPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 2;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvFormats.RTFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		deferredPSStream.RTVFormats = rtvFormats;

		deferredPSStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		TerrainDeferredPSO = PipelineState::Create("TerrainRender", Renderer.GetDevice(), deferredPSStream,
												   &Renderer.ShaderManager->BindlessRootSignature);
		struct TerraionDepthOnlyPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} depthOnlyPSStream;

		depthOnlyPSStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		depthOnlyPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());

		depthOnlyPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		depthOnlyPSStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		TerrainDepthOnlyPSO = PipelineState::Create("TerrainDepthOnly", Renderer.GetDevice(), depthOnlyPSStream,
													&Renderer.ShaderManager->BindlessRootSignature);
	}

	{
		struct WaterForwardPassPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendDesc;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilDesc;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} waterPSStream;

		waterPSStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		
		waterPSStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		waterPSStream.DepthStencilDesc = depthStencilDesc;
		auto blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		waterPSStream.BlendDesc = blendDesc;
		auto [vertexShader, pixelShader] = Renderer.ShaderManager->CompileBindlessGraphicsShader(
			L"RenderWater", RAD_SHADERS_DIR L"Compute/Terrain/RenderWater.hlsl");

		waterPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		waterPSStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		waterPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		waterPSStream.RTVFormats = rtvFormats;

		WaterForwardPSO = PipelineState::Create("WaterRender", Renderer.GetDevice(), waterPSStream,
												&Renderer.ShaderManager->BindlessRootSignature);
	}
	{
		struct TerraionRenderPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} waterPrepassPSStream;

		waterPrepassPSStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		auto [vertexShader, pixelShader] = Renderer.ShaderManager->CompileBindlessGraphicsShader(
			L"WaterPrepass", RAD_SHADERS_DIR L"Compute/Terrain/WaterPrepass.hlsl");

		waterPrepassPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		waterPrepassPSStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		waterPrepassPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		waterPrepassPSStream.RTVFormats = rtvFormats;

		waterPrepassPSStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		WaterPrePassPSO = PipelineState::Create("WaterPrepass", Renderer.GetDevice(), waterPrepassPSStream,
													&Renderer.ShaderManager->BindlessRootSignature);
	}

	return true;
}

std::vector<float> TerrainErosionSystem::CreateDiamondSquareHeightMap(uint32_t width, float roughness)
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

	// Get rid of rightmost column and bottom row to make it a power of two
	float min = FLT_MAX, max = FLT_MIN;
	std::vector<float> heightMapVals((width - 1) * (width - 1));
	for (int y = 0; y < width - 1; y++)
		for (int x = 0; x < width - 1; x++)
			heightMapVals[GetIndex(x, y, width - 1)] = heightMap(x, y);
	return heightMapVals;
}

CTerrain TerrainErosionSystem::CreateTerrain(uint32_t heightMapWidth)
{
	generator = std::mt19937(time(0));
	CTerrain terrain{};
	DXTexture::TextureCreateInfo baseTextureInfo = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Width = uint32_t(heightMapWidth),
		.Height = uint32_t(heightMapWidth),
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R32_FLOAT,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};
	terrain.HeightMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"HeightMap", baseTextureInfo));
	terrain.WaterHeightMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"WaterHeightMap", baseTextureInfo));
	terrain.SedimentMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"SedimentMap", baseTextureInfo));
	terrain.TempHeightMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"TempHeightMap", baseTextureInfo));
	terrain.TempSedimentMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"TempSedimentMap", baseTextureInfo));
	terrain.SoftnessMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"HardnessMap", baseTextureInfo));

	baseTextureInfo.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	terrain.WaterOutflux =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"WaterOutflux", baseTextureInfo));
	terrain.ThermalPipe1 =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"ThermalPipe1", baseTextureInfo));
	terrain.ThermalPipe2 =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"ThermalPipe2", baseTextureInfo));

	baseTextureInfo.Format = DXGI_FORMAT_R32G32_FLOAT;
	terrain.VelocityMap =
		std::make_shared<RWTexture>(DXTexture::Create(Renderer.GetDevice(), L"VelocityMap", baseTextureInfo));
	return terrain;
}

void TerrainErosionSystem::GenerateBaseHeightMap(CommandRecord& cmdRecord, CTerrain& terrain,
												 CErosionParameters const& parameters,
												 OptionalRef<CTerrainRenderable> terrainRenderable,
												 OptionalRef<CWaterRenderable> waterRenderable)
{
	// create heightmap
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
			data[i] = std::pow((data[i] - valsMin) * oneOverRange, 2.0f) * (max - min) + min;
	};
	if (parameters.BaseFromFile)
	{
		int width, height, channels;
		float* heightMapVals = stbi_loadf(RAD_ASSETS_DIR "heightmap.png", &width, &height, &channels, 1);
		scaleHeightMaps(heightMapVals, width * height, parameters.MinHeight, parameters.MaxHeight);
		cmdRecord.Push(
			"UploadHeightMap",
			[heightMap = terrain.HeightMap, heightMapVals, width, height, channels](CommandContext& cmdContext)
			{
				heightMap->UploadDataTyped<float>(cmdContext, std::span<const float>(heightMapVals, width * width));
				stbi_image_free(heightMapVals);
			});
	}
	else
	{
		if (!parameters.Random)
			generator = std::mt19937(parameters.Seed);
		else
			generator = std::mt19937(time(0));
		auto heightMapVals = CreateDiamondSquareHeightMap(terrain.HeightMap->Info.Width, parameters.InitialRoughness);
		scaleHeightMaps(heightMapVals.data(), heightMapVals.size(), parameters.MinHeight, parameters.MaxHeight);
		cmdRecord.Push("UploadHeightMap", [heightMap = terrain.HeightMap,
										   heightMapVals = std::move(heightMapVals)](CommandContext& cmdContext)
					   { heightMap->UploadDataTyped<float>(cmdContext, heightMapVals); });
	}

	cmdRecord.Push(
		"ClearMaps",
		[waterHeightMap = terrain.WaterHeightMap, sedimentMap = terrain.SedimentMap,
		 waterOutflux = terrain.WaterOutflux, softnessMap = terrain.SoftnessMap](CommandContext& cmdContext)
		{
			// Clear water/sediment/outflux/hardness maps
			TransitionVec()
				.Add(*waterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Add(*sedimentMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Add(*waterOutflux, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Add(*softnessMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Execute(cmdContext);
			float clearValue[4] = {0.f, 0.f, 0.f, 0.f};
			auto cpuUAV = g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = waterHeightMap->Info.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;
			uavDesc.Texture2D.PlaneSlice = 0;
			waterHeightMap->CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
			cmdContext->ClearUnorderedAccessViewFloat(waterHeightMap->UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(),
													  waterHeightMap->Resource.Get(), clearValue, 0, nullptr);
			uavDesc.Format = sedimentMap->Info.Format;
			sedimentMap->CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
			cmdContext->ClearUnorderedAccessViewFloat(sedimentMap->UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(),
													  sedimentMap->Resource.Get(), clearValue, 0, nullptr);
			uavDesc.Format = waterOutflux->Info.Format;
			waterOutflux->CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
			cmdContext->ClearUnorderedAccessViewFloat(waterOutflux->UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(),
													  waterOutflux->Resource.Get(), clearValue, 0, nullptr);
			uavDesc.Format = softnessMap->Info.Format;
			float hardnessClearValue[4] = {0.5f, 0.5f, 0.5f, 0.5f};
			softnessMap->CreatePlacedUAV(cpuUAV.GetView(), &uavDesc);
			cmdContext->ClearUnorderedAccessViewFloat(softnessMap->UAV.GetGPUHandle(), cpuUAV.GetCPUHandle(),
													  softnessMap->Resource.Get(), hardnessClearValue, 0, nullptr);
			g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StaticPage->Top--;
		});

	terrain.IterationCount = 0;
	if (terrainRenderable)
		GenerateTerrainMaterial(cmdRecord, terrain, parameters, *terrainRenderable);
	if (waterRenderable)
		GenerateWaterMaterial(cmdRecord, terrain, parameters, *waterRenderable);
}

void TerrainErosionSystem::ErodeTerrain(CommandRecord& cmdRecord, CTerrain& terrain,
										CErosionParameters const& parameters,
										OptionalRef<CTerrainRenderable> terrainRenderable,
										OptionalRef<CWaterRenderable> waterRenderable)
{
	for (int i = 0; i < parameters.Iterations; i++)
	{

		cmdRecord.Push(
			"Erosion",
			[heightMap = terrain.HeightMap, waterHeightMap = terrain.WaterHeightMap, sedimentMap = terrain.SedimentMap,
			 tempHeightMap = terrain.TempHeightMap, tempSedimentMap = terrain.TempSedimentMap,
			 softnessMap = terrain.SoftnessMap, thermalPipe1 = terrain.ThermalPipe1,
			 thermalPipe2 = terrain.ThermalPipe2, waterOutflux = terrain.WaterOutflux,
			 velocityMap = terrain.VelocityMap, parameters = CErosionParameters(parameters),
			 iterationCount = terrain.IterationCount, hydrolicAddWaterPSO = Ref(HydrolicAddWaterPSO),
			 hydrolicCalculateOutfluxPSO = Ref(HydrolicCalculateOutfluxPSO),
			 hydrolicUpdateWaterVelocityPSO = Ref(HydrolicUpdateWaterVelocityPSO),
			 hydrolicErosionAndDepositionPSO = Ref(HydrolicErosionAndDepositionPSO),
			 hydrolicSedimentTransportationAndEvaporationPSO = Ref(HydrolicSedimentTransportationAndEvaporationPSO),
			 thermalOutfluxPSO = Ref(ThermalOutfluxPSO),
			 thermalDepositPSO = Ref(ThermalDepositPSO)](CommandContext& commandCtx)
			{
				uint32_t width = heightMap->Info.Width;
				uint32_t height = heightMap->Info.Height;

				float pipeLength = parameters.TotalLength / width;
				float crossSection = parameters.PipeCrossSection * pipeLength * pipeLength;

				hlsl::ThermalOutfluxResources outfluxResources{
					.InHeightMapIndex = heightMap->SRV.Index,
					.InHardnessMapIndex = softnessMap->SRV.Index,
					.OutFluxTextureIndex1 = thermalPipe1->UAV.Index,
					.OutFluxTextureIndex2 = thermalPipe2->UAV.Index,
					.ThermalErosionRate = parameters.ThermalErosionRate,
					.PipeLength = pipeLength,
					.SoftnessTalusCoefficient = parameters.SoftnessTalusCoefficient,
					.MinTalusCoefficient = parameters.MinTalusCoefficient,
				};

				hlsl::ThermalDepositResources depositResources{
					.InFluxTextureIndex1 = thermalPipe1->SRV.Index,
					.InFluxTextureIndex2 = thermalPipe2->SRV.Index,
					.OutHeightMapIndex = heightMap->UAV.Index,
				};
				hlsl::HydrolicAddWaterResources addWaterResources{
					.WaterMapIndex = waterHeightMap->UAV.Index,
					.RainRate = parameters.RainRate,
					.Iteration = iterationCount,
				};
				TransitionVec().Add(*waterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Execute(commandCtx);
				hydrolicAddWaterPSO->ExecuteCompute(commandCtx, addWaterResources, width / 8, height / 8, 1);

				hlsl::HydrolicCalculateOutfluxResources calculateOutfluxResources{
					.InHeightMapIndex = heightMap->SRV.Index,
					.InWaterMapIndex = waterHeightMap->SRV.Index,
					.OutFluxTextureIndex = waterOutflux->UAV.Index,
					.PipeCrossSection = crossSection,
					.PipeLength = pipeLength,
				};
				TransitionVec()
					.Add(*heightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*waterHeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*waterOutflux, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Execute(commandCtx);
				hydrolicCalculateOutfluxPSO->ExecuteCompute(commandCtx, calculateOutfluxResources, width / 8,
															height / 8, 1);

				hlsl::HydrolicUpdateWaterVelocityResources updateWaterVelocityResources{
					.InFluxTextureIndex = waterOutflux->SRV.Index,
					.OutWaterMapIndex = waterHeightMap->UAV.Index,
					.OutVelocityMapIndex = velocityMap->UAV.Index,
					.PipeLength = pipeLength,
				};
				TransitionVec()
					.Add(*waterOutflux, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*waterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*velocityMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Execute(commandCtx);
				hydrolicUpdateWaterVelocityPSO->ExecuteCompute(commandCtx, updateWaterVelocityResources, width / 8,
															   height / 8, 1);

				hlsl::HydrolicErosionAndDepositionResources erosionAndDepositionResources{
					.InVelocityMapIndex = velocityMap->SRV.Index,
					.InOldHeightMapIndex = heightMap->SRV.Index,
					.InOutSoftnessMapIndex = softnessMap->UAV.Index,
					.OutHeightMapIndex = tempHeightMap->UAV.Index,
					.OutWaterMapIndex = waterHeightMap->UAV.Index,
					.OutSedimentMapIndex = sedimentMap->UAV.Index,
					.PipeLength = pipeLength,
					.SedimentCapacity = parameters.SedimentCapacity,
					.SoilSuspensionRate = parameters.SoilSuspensionRate,
					.SedimentDepositionRate = parameters.SedimentDepositionRate,
					.SoilHardeningRate = parameters.SoilHardeningRate,
					.SoilSofteningRate = parameters.SoilSofteningRate,
					.MinimumSoftness = parameters.MinimumSoilSoftness,
					.MaximalErosionDepth = parameters.MaximalErosionDepth,
				};
				TransitionVec()
					.Add(*velocityMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*heightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*waterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*tempHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*sedimentMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*softnessMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Execute(commandCtx);
				hydrolicErosionAndDepositionPSO->ExecuteCompute(commandCtx, erosionAndDepositionResources, width / 8,
																height / 8, 1);
				// Copy temp height map to height map
				TransitionVec()
					.Add(*tempHeightMap, D3D12_RESOURCE_STATE_COPY_SOURCE)
					.Add(*heightMap, D3D12_RESOURCE_STATE_COPY_DEST)
					.Execute(commandCtx);
				commandCtx->CopyResource(heightMap->Resource.Get(), tempHeightMap->Resource.Get());

				hlsl::HydrolicSedimentTransportationAndEvaporationResources
					sedimentTransportationAndEvaporationResources{
						.InVelocityMapIndex = velocityMap->SRV.Index,
						.InOldSedimentMapIndex = sedimentMap->SRV.Index,
						.OutSedimentMapIndex = tempSedimentMap->UAV.Index,
						.PipeLength = pipeLength,
						.InOutWaterMapIndex = waterHeightMap->UAV.Index,
						.EvaporationRate = parameters.EvaporationRate,
					};
				TransitionVec()
					.Add(*velocityMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*sedimentMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*tempSedimentMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*waterHeightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Execute(commandCtx);
				hydrolicSedimentTransportationAndEvaporationPSO->ExecuteCompute(
					commandCtx, sedimentTransportationAndEvaporationResources, width / 8, height / 8, 1);
				// Copy temp sediment map to sediment map
				TransitionVec()
					.Add(*tempSedimentMap, D3D12_RESOURCE_STATE_COPY_SOURCE)
					.Add(*sedimentMap, D3D12_RESOURCE_STATE_COPY_DEST)
					.Execute(commandCtx);
				commandCtx->CopyResource(sedimentMap->Resource.Get(), tempSedimentMap->Resource.Get());

				TransitionVec()
					.Add(*thermalPipe1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*thermalPipe2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*heightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*softnessMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Execute(commandCtx);
				thermalOutfluxPSO->ExecuteCompute(commandCtx, outfluxResources, width / 8, height / 8, 1);

				TransitionVec()
					.Add(*thermalPipe1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.Add(*thermalPipe2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Add(*heightMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Execute(commandCtx);
				thermalDepositPSO->ExecuteCompute(commandCtx, depositResources, width / 8, height / 8, 1);
			});
		terrain.IterationCount++;
	}

	if (terrainRenderable)
		GenerateTerrainMaterial(cmdRecord, terrain, parameters, *terrainRenderable);
	if (waterRenderable)
		GenerateWaterMaterial(cmdRecord, terrain, parameters, *waterRenderable);
}

CIndexedPlane TerrainErosionSystem::CreatePlane(CommandRecord& cmdRecord, uint32_t resX, uint32_t resY)
{
	std::vector<uint32_t> indices((resX - 1) * (resY - 1) * 6);
	for (uint32_t y = 0; y < resY - 1; y++)
		for (uint32_t x = 0; x < resX - 1; x++)
		{
			auto vtx1 = GetIndex(x, y, resX);
			auto vtx2 = GetIndex(x + 1, y, resX);
			auto vtx3 = GetIndex(x, y + 1, resX);
			auto vtx4 = GetIndex(x + 1, y + 1, resX);
			indices[6 * GetIndex(x, y, resX - 1) + 0] = vtx1;
			indices[6 * GetIndex(x, y, resX - 1) + 1] = vtx3;
			indices[6 * GetIndex(x, y, resX - 1) + 2] = vtx2;
			indices[6 * GetIndex(x, y, resX - 1) + 3] = vtx3;
			indices[6 * GetIndex(x, y, resX - 1) + 4] = vtx4;
			indices[6 * GetIndex(x, y, resX - 1) + 5] = vtx2;
		}

	CIndexedPlane plane{.ResX = resX, .ResY = resY};
	plane.Indices = std::make_shared<DXTypedBuffer<uint32_t>>(DXTypedBuffer<uint32_t>::Create(
		Renderer.GetDevice(), L"PlaneIdxBuffer", indices.size(), D3D12_HEAP_TYPE_DEFAULT));

	cmdRecord.Push("UploadPlaneIndices",
				   [indices = std::move(indices), idxBuf = plane.Indices](CommandContext& commandCtx)
				   { commandCtx.IntermediateResources.push_back(idxBuf->Upload(commandCtx, indices)); });
	auto& idxBufView = plane.IndexBufferView;
	idxBufView.BufferLocation = plane.Indices->Resource->GetGPUVirtualAddress();
	idxBufView.SizeInBytes = plane.Indices->Size;
	idxBufView.Format = DXGI_FORMAT_R32_UINT;
	return plane;
}

CTerrainRenderable TerrainErosionSystem::CreateTerrainRenderable(CTerrain& terrain)
{
	CTerrainRenderable renderable{};
	renderable.HeightMap = terrain.HeightMap;
	auto texInfo = DXTexture::TextureCreateInfo{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Width = 1024,
		.Height = 1024,
		.MipLevels = 0,
		.Format = DXGI_FORMAT_R16G16B16A16_UNORM,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};
	renderable.TerrainAlbedoTex =
		std::make_shared<RWTexture>(RWTexture(DXTexture::Create(Renderer.GetDevice(), L"TerrainAlbedo", texInfo), -1));
	renderable.TerrainNormalMap =
		std::make_shared<RWTexture>(RWTexture(DXTexture::Create(Renderer.GetDevice(), L"TerrainNormal", texInfo), -1));
	return renderable;
}

CWaterRenderable TerrainErosionSystem::CreateWaterRenderable(CTerrain& terrain)
{
	CWaterRenderable renderable{};
	renderable.HeightMap = terrain.HeightMap;
	renderable.WaterHeightMap = terrain.WaterHeightMap;
	auto texInfo = DXTexture::TextureCreateInfo{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Width = 1024,
		.Height = 1024,
		.MipLevels = 0,
		.Format = DXGI_FORMAT_R16G16B16A16_UNORM,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};
	renderable.WaterAlbedoMap =
		std::make_shared<RWTexture>(RWTexture(DXTexture::Create(Renderer.GetDevice(), L"WaterAlbedo", texInfo), -1));
	renderable.WaterNormalMap =
		std::make_shared<RWTexture>(RWTexture(DXTexture::Create(Renderer.GetDevice(), L"WaterNormal", texInfo), -1));
	return renderable;
}

void TerrainErosionSystem::GenerateTerrainMaterial(CommandRecord& cmdRecord, CTerrain& terrain,
												   CErosionParameters const& parameters, CTerrainRenderable& renderable)
{
	renderable.TotalLength = parameters.TotalLength;
	cmdRecord.Push("GenerateTerrainMaterial",
				   [terrainAlbedo = renderable.TerrainAlbedoTex, terrainNormal = renderable.TerrainNormalMap,
					heightMap = terrain.HeightMap, totalLength = parameters.TotalLength,
					dispatchSize = std::pair<uint32_t, uint32_t>(renderable.TerrainNormalMap->Info.Width / 8,
																 renderable.TerrainNormalMap->Info.Height / 8),
					pso = Ref(HeightMapToTerrainMaterialPSO), renderer = Ref(Renderer)](CommandContext& commandCtx)
				   {
					   TransitionVec()
						   .Add(*terrainAlbedo, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						   .Add(*terrainNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						   .Add(*heightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
						   .Execute(commandCtx);

					   hlsl::HeightToTerrainMaterialResources resources{
						   .HeightMapTextureIndex = heightMap->SRV.Index,
						   .TerrainAlbedoTextureIndex = terrainAlbedo->UAV.Index,
						   .TerrainNormalMapTextureIndex = terrainNormal->UAV.Index,
						   .TotalLength = totalLength,
					   };

					   pso->ExecuteCompute(commandCtx, resources, dispatchSize.first, dispatchSize.second, 1);

					   renderer->TextureManager->GenerateMips(commandCtx, *terrainAlbedo);
					   renderer->TextureManager->GenerateMips(commandCtx, *terrainNormal);
					   TransitionVec()
						   .Add(*terrainAlbedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
						   .Add(*terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
						   .Execute(commandCtx);
				   });
}

void TerrainErosionSystem::GenerateWaterMaterial(CommandRecord& cmdRecord, CTerrain& terrain,
												 CErosionParameters const& parameters, CWaterRenderable& renderable)
{
	renderable.TotalLength = parameters.TotalLength;
	cmdRecord.Push("GenerateWaterMaterial",
				   [waterAlbedo = renderable.WaterAlbedoMap, waterNormal = renderable.WaterNormalMap,
					heightMap = terrain.HeightMap, waterHeightMap = terrain.WaterHeightMap,
					sedimentMap = terrain.SedimentMap,
					totalLength = parameters.TotalLength,
					dispatchSize = std::pair<uint32_t, uint32_t>(renderable.WaterNormalMap->Info.Width / 8,
																 renderable.WaterNormalMap->Info.Height / 8),
					pso = Ref(HeightMapToWaterMaterialPSO), renderer = Ref(Renderer)](CommandContext& commandCtx)
				   {
					   TransitionVec()
						   .Add(*waterAlbedo, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						   .Add(*waterNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						   .Add(*heightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
						   .Add(*waterHeightMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
						   .Add(*sedimentMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
						   .Execute(commandCtx);

					   hlsl::HeightToWaterMaterialResources resources{
						   .HeightMapTextureIndex = heightMap->SRV.Index,
						   .WaterHeightMapTextureIndex = waterHeightMap->SRV.Index,
						   .SedimentMapTextureIndex = sedimentMap->SRV.Index,
						   .WaterAlbedoTextureIndex = waterAlbedo->UAV.Index,
						   .WaterNormalMapTextureIndex = waterNormal->UAV.Index,
						   .TotalLength = totalLength,
					   };

					   pso->ExecuteCompute(commandCtx, resources, dispatchSize.first, dispatchSize.second, 1);

					   renderer->TextureManager->GenerateMips(commandCtx, *waterNormal);
					   TransitionVec()
						   .Add(*waterAlbedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
						   .Add(*waterNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
						   .Execute(commandCtx);
				   });
}

void TerrainErosionSystem::Update(entt::registry& registry, InputManager& inputMan, RenderFrameRecord& frameRecord)
{
	auto erosionView = registry.view<CTerrain, CErosionParameters>();
	for (auto entity : erosionView)
	{
		auto& terrain = erosionView.get<CTerrain>(entity);
		auto& parameters = erosionView.get<CErosionParameters>(entity);
		auto* terrainRenderable = registry.try_get<CTerrainRenderable>(entity);
		auto* waterRenderable = registry.try_get<CWaterRenderable>(entity);
		if (inputMan.IsKeyPressed(SDL_SCANCODE_M))
			GenerateBaseHeightMap(frameRecord.CommandRecord, terrain, parameters, terrainRenderable, waterRenderable);
		if (parameters.ErodeEachFrame || inputMan.IsKeyPressed(SDL_SCANCODE_K))
			ErodeTerrain(frameRecord.CommandRecord, terrain, parameters, terrainRenderable, waterRenderable);
	}

	auto terrainRenderableView = registry.view<ecs::CSceneTransform, CIndexedPlane, CTerrainRenderable>();

	std::vector<TerrainRenderData> terrainRenderDataVec;

	for (auto entity : terrainRenderableView)
	{
		auto& transform = terrainRenderableView.get<ecs::CSceneTransform>(entity);
		auto& plane = terrainRenderableView.get<CIndexedPlane>(entity);
		auto& renderable = terrainRenderableView.get<CTerrainRenderable>(entity);

		TerrainRenderData terrainRenderData{};
		terrainRenderData.WorldMatrix = transform.GetWorldTransform().WorldMatrix;
		terrainRenderData.Resources = hlsl::TerrainRenderResources{
			.MeshResX = plane.ResX,
			.MeshResY = plane.ResY,
			.HeightMapTextureIndex = renderable.HeightMap->SRV.Index,
			.TerrainAlbedoTextureIndex = renderable.TerrainAlbedoTex->SRV.Index,
			.TerrainNormalMapTextureIndex = renderable.TerrainNormalMap->SRV.Index,
			.TotalLength = renderable.TotalLength,
		};
		terrainRenderData.IndexBufferView = plane.IndexBufferView;
		terrainRenderData.IndexCount = (plane.ResX - 1) * (plane.ResY - 1) * 6;
		terrainRenderDataVec.push_back(terrainRenderData);
	}

	frameRecord.Push(TypedRenderCommand<TerrainRenderData>{
		.Name = "TerrainRender",
		.Data = std::move(terrainRenderDataVec),
		.DepthOnlyPass = [this](auto span, auto view, auto passData) { TerrainDepthOnlyPass(span, view, passData); },
		.DeferredPass = [this](auto span, auto view, auto passData) { TerrainDeferredPass(span, view, passData); }});

	auto waterRenderableView = registry.view<ecs::CSceneTransform, CIndexedPlane, CWaterRenderable>();

	std::vector<WaterRenderData> waterRenderDataVec;

	for (auto entity : waterRenderableView)
	{
		auto& transform = waterRenderableView.get<ecs::CSceneTransform>(entity);
		auto& plane = waterRenderableView.get<CIndexedPlane>(entity);
		auto& renderable = waterRenderableView.get<CWaterRenderable>(entity);

		WaterRenderData waterRenderData{};
		waterRenderData.WorldMatrix = transform.GetWorldTransform().WorldMatrix;
		waterRenderData.Resources = hlsl::WaterRenderResources{
			.MeshResX = plane.ResX,
			.MeshResY = plane.ResY,
			.HeightMapTextureIndex = renderable.HeightMap->SRV.Index,
			.WaterHeightMapTextureIndex = renderable.WaterHeightMap->SRV.Index,
			.WaterAlbedoTextureIndex = renderable.WaterAlbedoMap->SRV.Index,
			.WaterNormalMapTextureIndex = renderable.WaterNormalMap->SRV.Index,
			.TotalLength = renderable.TotalLength,
		};
		waterRenderData.IndexBufferView = plane.IndexBufferView;
		waterRenderData.IndexCount = (plane.ResX - 1) * (plane.ResY - 1) * 6;
		waterRenderDataVec.push_back(waterRenderData);
	}

	frameRecord.Push(TypedRenderCommand<WaterRenderData>{.Name = "WaterRender",
														 .Data = std::move(waterRenderDataVec),
														 .WaterPass = [this](auto span, auto view, auto passData) { WaterPrepass(span, view, passData); },
														 .ForwardPass = [this](auto span, auto view, auto passData)
														 { WaterForwardPass(span, view, passData); }});
}

void TerrainErosionSystem::TerrainDepthOnlyPass(std::span<TerrainRenderData> renderObjects, const RenderView& view,
												DepthOnlyPassData& passData)
{
	auto& cmd = passData.CmdContext;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	TerrainDepthOnlyPSO.Bind(cmd);

	TerrainRenderData lastRenderData{};
	for (auto& renderObj : renderObjects)
	{
		if (renderObj.IndexBufferView.BufferLocation != lastRenderData.IndexBufferView.BufferLocation)
		{
			lastRenderData.IndexBufferView = renderObj.IndexBufferView;
			cmd->IASetIndexBuffer(&renderObj.IndexBufferView);
		}
		rad::hlsl::TerrainRenderResources renderResources = renderObj.Resources;
		renderResources.MVP = view.ViewProjectionMatrix * renderObj.WorldMatrix;
		renderResources.Normal = glm::transpose(glm::inverse(renderObj.WorldMatrix));
		TerrainDepthOnlyPSO.SetResources(cmd, renderResources);
		cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
	}
}

void TerrainErosionSystem::TerrainDeferredPass(std::span<TerrainRenderData> renderObjects, const RenderView& view,
											   DeferredPassData& passData)
{
	auto& cmd = passData.CmdContext;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	TerrainDeferredPSO.Bind(cmd);

	TerrainRenderData lastRenderData{};
	for (auto& renderObj : renderObjects)
	{
		if (renderObj.IndexBufferView.BufferLocation != lastRenderData.IndexBufferView.BufferLocation)
		{
			lastRenderData.IndexBufferView = renderObj.IndexBufferView;
			cmd->IASetIndexBuffer(&renderObj.IndexBufferView);
		}
		rad::hlsl::TerrainRenderResources renderResources = renderObj.Resources;
		renderResources.MVP = view.ViewProjectionMatrix * renderObj.WorldMatrix;
		renderResources.Normal = glm::transpose(glm::inverse(renderObj.WorldMatrix));
		TerrainDeferredPSO.SetResources(cmd, renderResources);
		cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
	}
}

void TerrainErosionSystem::WaterPrepass(std::span<WaterRenderData> renderObjects, const RenderView& view,
										WaterPassData& passData)
{
	auto& cmd = passData.CmdContext;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	WaterPrePassPSO.Bind(cmd);

	WaterRenderData lastRenderData{};
	for (auto& renderObj : renderObjects)
	{
		if (renderObj.IndexBufferView.BufferLocation != lastRenderData.IndexBufferView.BufferLocation)
		{
			lastRenderData.IndexBufferView = renderObj.IndexBufferView;
			cmd->IASetIndexBuffer(&renderObj.IndexBufferView);
		}
		rad::hlsl::WaterRenderResources renderResources = renderObj.Resources;
		renderResources.ModelMatrix = renderObj.WorldMatrix;
		renderResources.MVP = view.ViewProjectionMatrix * renderObj.WorldMatrix;
		renderResources.Normal = glm::transpose(glm::inverse(renderObj.WorldMatrix));
		renderResources.ViewPos = glm::vec4(view.ViewPosition, 0.0f);
		WaterPrePassPSO.SetResources(cmd, renderResources);
		cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
	}
}

void TerrainErosionSystem::WaterForwardPass(std::span<WaterRenderData> renderObjects, const RenderView& view,
											ForwardPassData& passData)
{
	auto& cmd = passData.CmdContext;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	WaterForwardPSO.Bind(cmd);

	WaterRenderData lastRenderData{};
	for (auto& renderObj : renderObjects)
	{
		if (renderObj.IndexBufferView.BufferLocation != lastRenderData.IndexBufferView.BufferLocation)
		{
			lastRenderData.IndexBufferView = renderObj.IndexBufferView;
			cmd->IASetIndexBuffer(&renderObj.IndexBufferView);
		}
		rad::hlsl::WaterRenderResources renderResources = renderObj.Resources;
		renderResources.ModelMatrix = renderObj.WorldMatrix;
		renderResources.MVP = view.ViewProjectionMatrix * renderObj.WorldMatrix;
		renderResources.Normal = glm::transpose(glm::inverse(renderObj.WorldMatrix));
		renderResources.ViewPos = glm::vec4(view.ViewPosition, 0.0f);
		renderResources.ReflectionResultTextureIndex = passData.InReflectionResultSRV.GetIndex();
		renderResources.RefractionResultTextureIndex = passData.InRefractionResultSRV.GetIndex();
		renderResources.ColorTextureIndex = passData.InColorSRV.GetIndex();

		WaterForwardPSO.SetResources(cmd, renderResources);
		cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
	}
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
} // namespace rad::proc
