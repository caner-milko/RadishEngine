#include "TerrainGenerator.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/TextureManager.h"
#include <random>
#include "Compute/Terrain/TerrainResources.hlsli"
#include "Compute/Terrain/TerrainConstantBuffers.hlsli"
#include "Systems.h"
#include "stb_image.h"
#include "Graphics/RenderGraphHelpers.h"
#include "Graphics/Pipelines/DeferredRenderingPipeline.h"

namespace rad::proc
{
constexpr std::array<float, 4> HardnessClearCol = {0.5f, 0.5f, 0.5f, 0.5f};
size_t GetIndex(size_t x, size_t y, size_t width)
{
	return x + y * width;
}

template <typename T>
struct MapVector : std::vector<T>
{
	size_t X, Y;
	MapVector(size_t x, size_t y) : std::vector<T>(x * y), X(x), Y(y) {}
	T& operator()(size_t x, size_t y) { return this->at(GetIndex(x, y, X)); }
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
		"HeightToTerrainMaterialPipeline",
		Renderer,
		RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/HeightMapToTerrainMaterial.hlsl");
	HeightMapToWaterMaterialPSO = PipelineState::CreateBindlessComputePipeline(
		"HeightToWaterMaterialPipeline",
		Renderer,
		RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/HeightMapToWaterMaterial.hlsl");

	ThermalOutfluxPSO = PipelineState::CreateBindlessComputePipeline(
		"ThermalErosionOutflux", Renderer, RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/T1ThermalOutflux.hlsl");
	ThermalDepositPSO = PipelineState::CreateBindlessComputePipeline(
		"ThermalErosionDeposit", Renderer, RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/T2ThermalDeposit.hlsl");

	HydrolicAddWaterPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicAddWater", Renderer, RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/H1AddWater.hlsl");
	HydrolicCalculateOutfluxPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicCalculateOutflux", Renderer, RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/H2CalculateOutflux.hlsl");
	HydrolicUpdateWaterVelocityPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicUpdateWaterVelocity", Renderer, RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/H3UpdateWaterVelocity.hlsl");
	HydrolicErosionAndDepositionPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicErosionAndDeposition",
		Renderer,
		RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/H4ErosionAndDeposition.hlsl");
	HydrolicSedimentTransportationAndEvaporationPSO = PipelineState::CreateBindlessComputePipeline(
		"HydrolicSedimentTransportationAndEvaporation",
		Renderer,
		RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/H5SedimentTransportationAndEvaporation.hlsl");

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
			L"RenderTerrain", RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/RenderTerrain.hlsl");

		deferredPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		deferredPSStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		deferredPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 2;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvFormats.RTFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		deferredPSStream.RTVFormats = rtvFormats;

		deferredPSStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		TerrainDeferredPSO = PipelineState::Create(
			"TerrainRender", Renderer.GetDevice(), deferredPSStream, &Renderer.ShaderManager->BindlessRootSignature);
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

		TerrainDepthOnlyPSO = PipelineState::Create("TerrainDepthOnly",
													Renderer.GetDevice(),
													depthOnlyPSStream,
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
			L"RenderWater", RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/RenderWater.hlsl");

		waterPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		waterPSStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		waterPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		waterPSStream.RTVFormats = rtvFormats;

		WaterForwardPSO = PipelineState::Create(
			"WaterRender", Renderer.GetDevice(), waterPSStream, &Renderer.ShaderManager->BindlessRootSignature);
	}
	{
		struct WaterPrepassPipelineStateStream : PipelineStateStreamBase
		{
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilDesc;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} waterPrepassPSStream;

		waterPrepassPSStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		auto [vertexShader, pixelShader] = Renderer.ShaderManager->CompileBindlessGraphicsShader(
			L"WaterPrepass", RAD_ENGINE_SHADERS_DIR L"Compute/Terrain/WaterPrepass.hlsl");

		waterPrepassPSStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Blob.Get());
		waterPrepassPSStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Blob.Get());

		auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		waterPrepassPSStream.DepthStencilDesc = depthStencilDesc;
		waterPrepassPSStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		waterPrepassPSStream.RTVFormats = rtvFormats;

		waterPrepassPSStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		WaterPrePassPSO = PipelineState::Create(
			"WaterPrepass", Renderer.GetDevice(), waterPrepassPSStream, &Renderer.ShaderManager->BindlessRootSignature);
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
	auto createInfo = ResourceCreateHelper::Texture2D(
		heightMapWidth, heightMapWidth, DXGI_FORMAT_R32_FLOAT, ResourcePresetFlags::UnorderedAccess);
	auto rgbaCreateInfo = ResourceCreateHelper::Texture2D(
		heightMapWidth, heightMapWidth, DXGI_FORMAT_R32G32B32A32_FLOAT, ResourcePresetFlags::UnorderedAccess);
	auto rgCreateInfo = ResourceCreateHelper::Texture2D(
		heightMapWidth, heightMapWidth, DXGI_FORMAT_R32G32_FLOAT, ResourcePresetFlags::UnorderedAccess);
	return CTerrain{
		.HeightMap = Renderer.ResourcePool->GetResource(createInfo, "HeightMap"),
		.WaterHeightMap = Renderer.ResourcePool->GetResource(createInfo, "WaterHeightMap"),
		.SedimentMap = Renderer.ResourcePool->GetResource(createInfo, "SedimentMap"),
		.WaterOutflux = Renderer.ResourcePool->GetResource(rgbaCreateInfo, "WaterOutflux"),
		.VelocityMap = Renderer.ResourcePool->GetResource(rgCreateInfo, "VelocityMap"),
		.ThermalPipe1 = Renderer.ResourcePool->GetResource(rgbaCreateInfo, "ThermalPipe1"),
		.ThermalPipe2 = Renderer.ResourcePool->GetResource(rgbaCreateInfo, "ThermalPipe2"),
		.HardnessMap = Renderer.ResourcePool->GetResource(rgbaCreateInfo, "HardnessMap"),
	};
}

void TerrainErosionSystem::GenerateBaseHeightMap(RenderGraphBuilder& rgBuilder,
												 CTerrain& terrain,
												 CErosionParameters const& parameters,
												 OptionalRef<CTerrainRenderable> terrainRenderable,
												 OptionalRef<CWaterRenderable> waterRenderable)
{
	// create heightmap
	constexpr auto scaleHeightMaps = [](float* data, size_t size, float min, float max) {
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
	auto rgHeightMap = rgBuilder.GetOrAddExternalResource(terrain.HeightMap->AsView());
	if (parameters.BaseFromFile)
	{
		int width, height, channels;
		float* heightMapVals = stbi_loadf(RAD_ENGINE_ASSETS_DIR "heightmap.png", &width, &height, &channels, 1);
		scaleHeightMaps(heightMapVals, width * height, parameters.MinHeight, parameters.MaxHeight);
		std::vector<float> heightMapValsVec(heightMapVals, heightMapVals + width * height);
		stbi_image_free(heightMapVals);
		rghelpers::UploadTextureData(rgBuilder, rgHeightMap, std::move(heightMapValsVec));
	}
	else
	{
		if (!parameters.Random)
			generator = std::mt19937(parameters.Seed);
		else
			generator = std::mt19937(time(0));
		auto heightMapVals =
			CreateDiamondSquareHeightMap(terrain.HeightMap->Info().CreateInfo.Desc.Width, parameters.InitialRoughness);
		scaleHeightMaps(heightMapVals.data(), heightMapVals.size(), parameters.MinHeight, parameters.MaxHeight);
		rghelpers::UploadTextureData(rgBuilder, rgHeightMap, std::move(heightMapVals));
	}

	auto rgWaterHeightMap = rgBuilder.GetOrAddExternalResource(terrain.WaterHeightMap->AsView());
	auto rgSedimentMap = rgBuilder.GetOrAddExternalResource(terrain.SedimentMap->AsView());
	auto rgWaterOutflux = rgBuilder.GetOrAddExternalResource(terrain.WaterOutflux->AsView());
	auto rgHardnessMap = rgBuilder.GetOrAddExternalResource(terrain.HardnessMap->AsView());

	rghelpers::ClearUnorderedAccessViewFloat(rgBuilder, rgWaterHeightMap, {0.f, 0.f, 0.f, 0.f});
	rghelpers::ClearUnorderedAccessViewFloat(rgBuilder, rgSedimentMap, {0.f, 0.f, 0.f, 0.f});
	rghelpers::ClearUnorderedAccessViewFloat(rgBuilder, rgWaterOutflux, {0.f, 0.f, 0.f, 0.f});
	rghelpers::ClearUnorderedAccessViewFloat(rgBuilder, rgHardnessMap, HardnessClearCol);

	terrain.IterationCount = 0;
	if (terrainRenderable)
		GenerateTerrainMaterial(rgBuilder, terrain, parameters, *terrainRenderable);
	if (waterRenderable)
		GenerateWaterMaterial(rgBuilder, terrain, parameters, *waterRenderable);
}

void TerrainErosionSystem::ErodeTerrain(RenderGraphBuilder& rgBuilder,
										CTerrain& terrain,
										CErosionParameters const& parameters,
										OptionalRef<CTerrainRenderable> terrainRenderable,
										OptionalRef<CWaterRenderable> waterRenderable)
{
	Ref<RGBOutputResource> heightMap = rgBuilder.GetOrAddExternalResource(terrain.HeightMap->AsView());
	Ref<RGBOutputResource> waterHeightMap = rgBuilder.GetOrAddExternalResource(terrain.WaterHeightMap->AsView());
	Ref<RGBOutputResource> sedimentMap = rgBuilder.GetOrAddExternalResource(terrain.SedimentMap->AsView());
	Ref<RGBOutputResource> hardnessMap = rgBuilder.GetOrAddExternalResource(terrain.HardnessMap->AsView());
	Ref<RGBOutputResource> thermalPipe1 = rgBuilder.GetOrAddExternalResource(terrain.ThermalPipe1->AsView());
	Ref<RGBOutputResource> thermalPipe2 = rgBuilder.GetOrAddExternalResource(terrain.ThermalPipe2->AsView());
	Ref<RGBOutputResource> waterOutflux = rgBuilder.GetOrAddExternalResource(terrain.WaterOutflux->AsView());
	Ref<RGBOutputResource> velocityMap = rgBuilder.GetOrAddExternalResource(terrain.VelocityMap->AsView());
	Ref<RGBOutputResource> tempHeightMap =
		rgBuilder.AddGraphResource("TempHeightMap", terrain.HeightMap->Info().CreateInfo);
	Ref<RGBOutputResource> tempSedimentMap =
		rgBuilder.AddGraphResource("TempSedimentMap", terrain.SedimentMap->Info().CreateInfo);
	for (int i = 0; i < parameters.Iterations; i++)
	{
		uint32_t width = terrain.HeightMap->Info().CreateInfo.Desc.Width;
		uint32_t height = terrain.HeightMap->Info().CreateInfo.Desc.Height;
		auto iterationCount = terrain.IterationCount;
		float pipeLength = parameters.TotalLength / width;
		float crossSection = parameters.PipeCrossSection * pipeLength * pipeLength;
		{
			auto& addWaterPass = rgBuilder.AddPass("AddWater");
			auto inWater = addWaterPass.AddInResourceSetOut(terrain.WaterHeightMap->GetName(),
															waterHeightMap,
															RGResourceUsage::UnorderedAccessView(waterHeightMap));
			addWaterPass.Execute = [this, width, height, parameters, inWater, iterationCount](CommandContext& cmd) {
				hlsl::HydrolicAddWaterResources addWaterResources{
					.WaterMapIndex = inWater->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.RainRate = parameters.RainRate,
					.DeltaTime = parameters.DeltaTime,
					.Iteration = iterationCount,
				};

				HydrolicAddWaterPSO.ExecuteCompute(cmd, addWaterResources, width / 8, height / 8, 1);
			};
		}
		{
			auto& calculateOutfluxPass = rgBuilder.AddPass("CalculateOutflux");
			auto inHeight = calculateOutfluxPass.AddInResourceSetOut(
				terrain.HeightMap->GetName(), heightMap, RGResourceUsage::NonPixelShaderResourceView(heightMap));
			auto inWater =
				calculateOutfluxPass.AddInResourceSetOut(terrain.WaterHeightMap->GetName(),
														 waterHeightMap,
														 RGResourceUsage::NonPixelShaderResourceView(waterHeightMap));
			auto outFlux = calculateOutfluxPass.AddInResourceSetOut(
				terrain.WaterOutflux->GetName(), waterOutflux, RGResourceUsage::UnorderedAccessView(waterOutflux));
			calculateOutfluxPass.Execute = [=](CommandContext& cmd) {
				hlsl::HydrolicCalculateOutfluxResources calculateOutfluxResources{
					.InHeightMapIndex = inHeight->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.InWaterMapIndex = inWater->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.OutFluxTextureIndex = outFlux->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.PipeCrossSection = crossSection,
					.PipeLength = pipeLength,
					.DeltaTime = parameters.DeltaTime};
				HydrolicCalculateOutfluxPSO.ExecuteCompute(cmd, calculateOutfluxResources, width / 8, height / 8, 1);
			};
		}
		{
			auto& updateWaterVelocityPass = rgBuilder.AddPass("UpdateWaterVelocity");
			auto inFlux =
				updateWaterVelocityPass.AddInResourceSetOut(terrain.WaterOutflux->GetName(),
															waterOutflux,
															RGResourceUsage::NonPixelShaderResourceView(waterOutflux));
			auto outWater =
				updateWaterVelocityPass.AddInResourceSetOut(terrain.WaterHeightMap->GetName(),
															waterHeightMap,
															RGResourceUsage::UnorderedAccessView(waterHeightMap));
			auto outVelocity = updateWaterVelocityPass.AddInResourceSetOut(
				terrain.VelocityMap->GetName(), velocityMap, RGResourceUsage::UnorderedAccessView(velocityMap));
			updateWaterVelocityPass.Execute = [=](CommandContext& cmd) {
				hlsl::HydrolicUpdateWaterVelocityResources updateWaterVelocityResources{
					.InFluxTextureIndex = inFlux->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.OutWaterMapIndex = outWater->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.OutVelocityMapIndex =
						outVelocity->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.PipeLength = pipeLength,
					.DeltaTime = parameters.DeltaTime};
				HydrolicUpdateWaterVelocityPSO.ExecuteCompute(
					cmd, updateWaterVelocityResources, width / 8, height / 8, 1);
			};
		}
		{
			auto& erosionAndDepositionPass = rgBuilder.AddPass("ErosionAndDeposition");

			auto inVelocity = erosionAndDepositionPass.AddInResourceSetOut(
				terrain.VelocityMap->GetName(), velocityMap, RGResourceUsage::NonPixelShaderResourceView(velocityMap));
			auto inHeight = erosionAndDepositionPass.AddInResourceSetOut(
				terrain.HeightMap->GetName(), heightMap, RGResourceUsage::NonPixelShaderResourceView(heightMap));
			auto inWater = erosionAndDepositionPass.AddInResourceSetOut(
				terrain.WaterHeightMap->GetName(),
				waterHeightMap,
				RGResourceUsage::NonPixelShaderResourceView(waterHeightMap));
			auto outHardness = erosionAndDepositionPass.AddInResourceSetOut(
				terrain.HardnessMap->GetName(), hardnessMap, RGResourceUsage::UnorderedAccessView(hardnessMap));
			auto outTempHeight = erosionAndDepositionPass.AddInResourceSetOut(
				tempHeightMap->Name, tempHeightMap, RGResourceUsage::UnorderedAccessView(tempHeightMap));
			auto outSediment = erosionAndDepositionPass.AddInResourceSetOut(
				terrain.SedimentMap->GetName(), sedimentMap, RGResourceUsage::UnorderedAccessView(sedimentMap));
			erosionAndDepositionPass.Execute = [&](CommandContext& cmd) {
				hlsl::HydrolicErosionAndDepositionResources erosionAndDepositionResources{
					.InVelocityMapIndex = inVelocity->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.InOldHeightMapIndex = inHeight->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.InOutHardnessMapIndex =
						outHardness->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.OutHeightMapIndex =
						outTempHeight->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.OutWaterMapIndex = inWater->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.OutSedimentMapIndex =
						outSediment->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.PipeLength = pipeLength,
					.SedimentCapacity = parameters.SedimentCapacity,
					.SoilSuspensionRate = parameters.SoilSuspensionRate,
					.SedimentDepositionRate = parameters.SedimentDepositionRate,
					.SoilHardeningRate = parameters.SoilHardeningRate,
					.MaximumHardness = parameters.MaximumSoilHardness,
					.MaximalErosionDepth = parameters.MaximalErosionDepth,
					.DeltaTime = parameters.DeltaTime};
				HydrolicErosionAndDepositionPSO.ExecuteCompute(
					cmd, erosionAndDepositionResources, width / 8, height / 8, 1);
			};
			rghelpers::CopyResource(rgBuilder, tempHeightMap, heightMap);
		}
		{
			auto& sedimentTransportationAndEvaporationPass = rgBuilder.AddPass("SedimentTransportationAndEvaporation");
			auto inVelocity = sedimentTransportationAndEvaporationPass.AddInResourceSetOut(
				terrain.VelocityMap->GetName(), velocityMap, RGResourceUsage::NonPixelShaderResourceView(velocityMap));
			auto passOldSediment = sedimentTransportationAndEvaporationPass.AddInResourceSetOut(
				terrain.SedimentMap->GetName(), sedimentMap, RGResourceUsage::NonPixelShaderResourceView(sedimentMap));
			auto passTempSediment = sedimentTransportationAndEvaporationPass.AddInResourceSetOut(
				tempSedimentMap->Name, tempSedimentMap, RGResourceUsage::UnorderedAccessView(tempSedimentMap));
			auto inWater = sedimentTransportationAndEvaporationPass.AddInResourceSetOut(
				terrain.WaterHeightMap->GetName(),
				waterHeightMap,
				RGResourceUsage::UnorderedAccessView(waterHeightMap));
			sedimentTransportationAndEvaporationPass.Execute = [=](CommandContext& cmd) {
				hlsl::HydrolicSedimentTransportationAndEvaporationResources
					sedimentTransportationAndEvaporationResources{
						.InVelocityMapIndex =
							inVelocity->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
						.InOldSedimentMapIndex =
							passOldSediment->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
						.OutSedimentMapIndex =
							passTempSediment->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
						.PipeLength = pipeLength,
						.InOutWaterMapIndex =
							inWater->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
						.EvaporationRate = parameters.EvaporationRate,
						.DeltaTime = parameters.DeltaTime};
				HydrolicSedimentTransportationAndEvaporationPSO.ExecuteCompute(
					cmd, sedimentTransportationAndEvaporationResources, width / 8, height / 8, 1);
			};
			rghelpers::CopyResource(rgBuilder, tempSedimentMap, sedimentMap);
		}
		{
			auto& thermalOutfluxPass = rgBuilder.AddPass("ThermalOutflux");
			auto inHeight = thermalOutfluxPass.AddInResourceSetOut(
				terrain.HeightMap->GetName(), heightMap, RGResourceUsage::NonPixelShaderResourceView(heightMap));
			auto inHardness = thermalOutfluxPass.AddInResourceSetOut(
				terrain.HardnessMap->GetName(), hardnessMap, RGResourceUsage::NonPixelShaderResourceView(hardnessMap));
			auto outFlux1 = thermalOutfluxPass.AddInResourceSetOut(
				terrain.ThermalPipe1->GetName(), thermalPipe1, RGResourceUsage::UnorderedAccessView(thermalPipe1));
			auto outFlux2 = thermalOutfluxPass.AddInResourceSetOut(
				terrain.ThermalPipe2->GetName(), thermalPipe2, RGResourceUsage::UnorderedAccessView(thermalPipe2));
			thermalOutfluxPass.Execute = [=](CommandContext& cmd) {
				hlsl::ThermalOutfluxResources outfluxResources{
					.InHeightMapIndex = inHeight->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.InHardnessMapIndex = inHardness->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.OutFluxTextureIndex1 =
						outFlux1->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.OutFluxTextureIndex2 =
						outFlux2->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
					.ThermalErosionRate = parameters.ThermalErosionRate,
					.PipeLength = pipeLength,
					.TalusAngleTangentCoeff = parameters.TalusAngleTangentCoeff,
					.TalusAngleTangentBias = parameters.TalusAngleTangentBias,
					.DeltaTime = parameters.DeltaTime};
				ThermalOutfluxPSO.ExecuteCompute(cmd, outfluxResources, width / 8, height / 8, 1);
			};
		}
		{
			auto& thermalDepositPass = rgBuilder.AddPass("ThermalDeposit");
			auto inFlux1 =
				thermalDepositPass.AddInResourceSetOut(terrain.ThermalPipe1->GetName(),
													   thermalPipe1,
													   RGResourceUsage::NonPixelShaderResourceView(thermalPipe1));
			auto inFlux2 =
				thermalDepositPass.AddInResourceSetOut(terrain.ThermalPipe2->GetName(),
													   thermalPipe2,
													   RGResourceUsage::NonPixelShaderResourceView(thermalPipe2));
			auto outHeight = thermalDepositPass.AddInResourceSetOut(
				terrain.HeightMap->GetName(), heightMap, RGResourceUsage::UnorderedAccessView(heightMap));
			thermalDepositPass.Execute = [=](CommandContext& cmd) {
				hlsl::ThermalDepositResources depositResources{
					.InFluxTextureIndex1 = inFlux1->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.InFluxTextureIndex2 = inFlux2->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
					.OutHeightMapIndex = outHeight->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
				};
				ThermalDepositPSO.ExecuteCompute(cmd, depositResources, width / 8, height / 8, 1);
			};
		}
		terrain.IterationCount++;
	}

	if (terrainRenderable)
		GenerateTerrainMaterial(rgBuilder, terrain, parameters, *terrainRenderable);
	if (waterRenderable)
		GenerateWaterMaterial(rgBuilder, terrain, parameters, *waterRenderable);
}

CIndexedPlane TerrainErosionSystem::CreatePlane(RenderGraphBuilder& rgBuilder, uint32_t resX, uint32_t resY)
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

	auto& indicesRes = Renderer.ResourcePool->GetResource(
		ResourceCreateHelper::Buffer(indices.size() * sizeof(uint32_t), ResourcePresetFlags::IndexBuffer),
		"PlaneIdxBuffer");
	CIndexedPlane plane{.ResX = resX, .ResY = resY, .Indices = indicesRes};
	auto rgIndices = rgBuilder.GetOrAddExternalResource(plane.Indices->AsView());
	rghelpers::UploadBufferData(rgBuilder, rgIndices, indices);
	return plane;
}

CTerrainRenderable TerrainErosionSystem::CreateTerrainRenderable(CTerrain& terrain)
{
	CTerrainRenderable renderable{
		.HeightMap = terrain.HeightMap->AsView(),
		.TerrainAlbedoTex = Renderer.ResourcePool->GetResource(
			ResourceCreateHelper::Texture2D(1024,
											1024,
											DXGI_FORMAT_R16G16B16A16_UNORM,
											ResourcePresetFlags::UnorderedAccess |
												ResourcePresetFlags::MipMappedTexture),
			"TerrainAlbedo"),
		.TerrainNormalMap = Renderer.ResourcePool->GetResource(
			ResourceCreateHelper::Texture2D(1024,
											1024,
											DXGI_FORMAT_R16G16B16A16_UNORM,
											ResourcePresetFlags::UnorderedAccess |
												ResourcePresetFlags::MipMappedTexture),
			"TerrainNormal"),
	};
	return renderable;
}

CWaterRenderable TerrainErosionSystem::CreateWaterRenderable(CTerrain& terrain)
{
	CWaterRenderable renderable{
		.HeightMap = terrain.HeightMap->AsView(),
		.WaterHeightMap = terrain.WaterHeightMap->AsView(),
		.WaterAlbedoMap = Renderer.ResourcePool->GetResource(
			ResourceCreateHelper::Texture2D(1024,
											1024,
											DXGI_FORMAT_R16G16B16A16_UNORM,
											ResourcePresetFlags::UnorderedAccess |
												ResourcePresetFlags::MipMappedTexture),
			"WaterAlbedo"),
		.WaterNormalMap = Renderer.ResourcePool->GetResource(
			ResourceCreateHelper::Texture2D(1024,
											1024,
											DXGI_FORMAT_R16G16B16A16_UNORM,
											ResourcePresetFlags::UnorderedAccess |
												ResourcePresetFlags::MipMappedTexture),
			"WaterNormal"),

	};
	return renderable;
}

void TerrainErosionSystem::GenerateTerrainMaterial(RenderGraphBuilder& rgBuilder,
												   CTerrain& terrain,
												   CErosionParameters const& parameters,
												   CTerrainRenderable& renderable)
{
	renderable.TotalLength = parameters.TotalLength;

	auto& pass = rgBuilder.AddPass("GenerateTerrainMaterial");
	auto terrainAlbedo = rgBuilder.GetOrAddExternalResource(renderable.TerrainAlbedoTex->AsView());
	auto terrainNormal = rgBuilder.GetOrAddExternalResource(renderable.TerrainNormalMap->AsView());
	auto heightMap = rgBuilder.GetOrAddExternalResource(terrain.HeightMap->AsView());

	auto inTerrainAlbedo = pass.AddInResourceSetOut(
		renderable.TerrainAlbedoTex->GetName(), terrainAlbedo, RGResourceUsage::UnorderedAccessView(terrainAlbedo));
	auto inTerrainNormal = pass.AddInResourceSetOut(
		renderable.TerrainNormalMap->GetName(), terrainNormal, RGResourceUsage::UnorderedAccessView(terrainNormal));
	auto inHeightMap = pass.AddInResourceSetOut(
		terrain.HeightMap->GetName(), heightMap, RGResourceUsage::NonPixelShaderResourceView(heightMap));

	pass.Execute = [=](CommandContext& commandCtx) {
		hlsl::HeightToTerrainMaterialResources resources{
			.HeightMapTextureIndex = inHeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
			.TerrainAlbedoTextureIndex =
				inTerrainAlbedo->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
			.TerrainNormalMapTextureIndex =
				inTerrainNormal->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
			.TotalLength = parameters.TotalLength,
		};
		HeightMapToTerrainMaterialPSO.ExecuteCompute(commandCtx,
													 resources,
													 inTerrainNormal->GetResourceView().GetCreateInfo().Desc.Width / 8,
													 inTerrainNormal->GetResourceView().GetCreateInfo().Desc.Height / 8,
													 1);
	};
	Renderer.TextureManager->GenerateMips(rgBuilder, terrainAlbedo);
	Renderer.TextureManager->GenerateMips(rgBuilder, terrainAlbedo);
}

void TerrainErosionSystem::GenerateWaterMaterial(RenderGraphBuilder& rgBuilder,
												 CTerrain& terrain,
												 CErosionParameters const& parameters,
												 CWaterRenderable& renderable)
{
	renderable.TotalLength = parameters.TotalLength;

	auto& pass = rgBuilder.AddPass("GenerateWaterMaterial");
	auto waterAlbedo = rgBuilder.GetOrAddExternalResource(renderable.WaterAlbedoMap->AsView());
	auto waterNormal = rgBuilder.GetOrAddExternalResource(renderable.WaterNormalMap->AsView());
	auto heightMap = rgBuilder.GetOrAddExternalResource(terrain.HeightMap->AsView());
	auto waterHeightMap = rgBuilder.GetOrAddExternalResource(terrain.WaterHeightMap->AsView());
	auto sedimentMap = rgBuilder.GetOrAddExternalResource(terrain.SedimentMap->AsView());
	auto inWaterAlbedo = pass.AddInResourceSetOut(
		renderable.WaterAlbedoMap->GetName(), waterAlbedo, RGResourceUsage::UnorderedAccessView(waterAlbedo));
	auto inWaterNormal = pass.AddInResourceSetOut(
		renderable.WaterNormalMap->GetName(), waterNormal, RGResourceUsage::UnorderedAccessView(waterNormal));
	auto inHeightMap =
		pass.AddInput(terrain.HeightMap->GetName(), heightMap, RGResourceUsage::NonPixelShaderResourceView(heightMap));
	auto inWaterHeightMap = pass.AddInput(
		terrain.WaterHeightMap->GetName(), waterHeightMap, RGResourceUsage::NonPixelShaderResourceView(waterHeightMap));
	auto inSedimentMap = pass.AddInput(
		terrain.SedimentMap->GetName(), sedimentMap, RGResourceUsage::NonPixelShaderResourceView(sedimentMap));
	pass.Execute = [=](CommandContext& commandCtx) {
		hlsl::HeightToWaterMaterialResources resources{
			.HeightMapTextureIndex = inHeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
			.WaterHeightMapTextureIndex =
				inWaterHeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
			.SedimentMapTextureIndex = inSedimentMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index,
			.WaterAlbedoTextureIndex =
				inWaterAlbedo->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
			.WaterNormalMapTextureIndex =
				inWaterNormal->GetResourceView().AsGPUDescriptor<UnorderedAccessViewDesc>().Index,
			.TotalLength = parameters.TotalLength,
		};
		HeightMapToWaterMaterialPSO.ExecuteCompute(commandCtx,
												   resources,
												   inWaterNormal->GetResourceView().GetCreateInfo().Desc.Width / 8,
												   inWaterNormal->GetResourceView().GetCreateInfo().Desc.Height / 8,
												   1);
	};
	Renderer.TextureManager->GenerateMips(rgBuilder, waterNormal);
}

void TerrainErosionSystem::Update(entt::registry& registry, InputManager& inputMan)
{
	auto erosionView =
		registry.view<ecs::CSceneTransform, CIndexedPlane, CTerrainRenderable, CTerrain, CErosionParameters>();
	for (auto entity : erosionView)
	{
		auto& terrain = erosionView.get<CTerrain>(entity);
		auto& parameters = erosionView.get<CErosionParameters>(entity);
		auto* terrainRenderable = registry.try_get<CTerrainRenderable>(entity);
		auto* waterRenderable = registry.try_get<CWaterRenderable>(entity);
		if (inputMan.IsKeyPressed(SDL_SCANCODE_M))
		{
			Renderer.DeferredPipeline->EnqueuedPreRenderFuncs.push(
				[this, &terrain, &parameters, terrainRenderable, waterRenderable](PreRenderPassData& passData) {
					GenerateBaseHeightMap(
						passData.GraphBuilder, terrain, parameters, terrainRenderable, waterRenderable);
				});
		}
		if (parameters.ErodeEachFrame || inputMan.IsKeyPressed(SDL_SCANCODE_K))
		{
			Renderer.DeferredPipeline->EnqueuedPreRenderFuncs.push(
				[this, &terrain, &parameters, terrainRenderable, waterRenderable](PreRenderPassData& passData) {
					ErodeTerrain(passData.GraphBuilder, terrain, parameters, terrainRenderable, waterRenderable);
				});
		}
	}

	auto terrainRenderableView = registry.view<ecs::CSceneTransform, CIndexedPlane, CTerrainRenderable>();

	FrameTerrainRenderData.clear();
	for (auto entity : terrainRenderableView)
	{
		auto& transform = terrainRenderableView.get<ecs::CSceneTransform>(entity);
		auto& plane = terrainRenderableView.get<CIndexedPlane>(entity);
		auto& renderable = terrainRenderableView.get<CTerrainRenderable>(entity);

		TerrainRenderData terrainRenderData{.WorldMatrix = transform.GetWorldTransform().WorldMatrix,
											.IndexCount = (plane.ResX - 1) * (plane.ResY - 1) * 6,
											.Indices = plane.Indices->AsView(),
											.HeightMap = renderable.HeightMap,
											.TerrainAlbedoTex = renderable.TerrainAlbedoTex->AsView(),
											.TerrainNormalMap = renderable.TerrainNormalMap->AsView()};
		terrainRenderData.Resources = hlsl::TerrainRenderResources{
			.MeshResX = plane.ResX,
			.MeshResY = plane.ResY,
			.TotalLength = renderable.TotalLength,
		};
		FrameTerrainRenderData.push_back(std::move(terrainRenderData));
	}

	auto waterRenderableView = registry.view<ecs::CSceneTransform, CIndexedPlane, CWaterRenderable>();

	FrameWaterRenderData.clear();
	for (auto entity : waterRenderableView)
	{
		auto& transform = waterRenderableView.get<ecs::CSceneTransform>(entity);
		auto& plane = waterRenderableView.get<CIndexedPlane>(entity);
		auto& renderable = waterRenderableView.get<CWaterRenderable>(entity);

		WaterRenderData waterRenderData{
			.WorldMatrix = transform.GetWorldTransform().WorldMatrix,
			.IndexCount = (plane.ResX - 1) * (plane.ResY - 1) * 6,
			.Indices = plane.Indices->AsView(),
			.HeightMap = renderable.HeightMap,
			.WaterHeightMap = renderable.WaterHeightMap,
			.WaterAlbedoMap = renderable.WaterAlbedoMap->AsView(),
			.WaterNormalMap = renderable.WaterNormalMap->AsView(),
		};
		waterRenderData.Resources = hlsl::WaterRenderResources{
			.MeshResX = plane.ResX,
			.MeshResY = plane.ResY,
			.TotalLength = renderable.TotalLength,
		};
		FrameWaterRenderData.push_back(waterRenderData);
	}
}

void TerrainErosionSystem::TerrainShadowMapPass(ShadowMapPassData& passData)
{
	struct RenderObject
	{
		uint32_t IndexCount;
		Ref<RGBInputResource> IndexBuffer;
		Ref<RGBInputResource> HeightMap;
		Ref<RGBInputResource> AlbedoMap;
		Ref<RGBInputResource> NormalMap;
		hlsl::TerrainRenderResources Resources;
	};

	auto& pass = passData.GraphBuilder.AddPass("TerrainShadowMapPass");

	std::vector<RenderObject> renderObjects;
	renderObjects.reserve(FrameTerrainRenderData.size());
	for (auto& renderData : FrameTerrainRenderData)
	{
		glm::mat4 mvp = passData.Frame.LightInfo.View.ViewProjectionMatrix * renderData.WorldMatrix;
		glm::mat4 normalMatrix = glm::transpose(glm::inverse(renderData.WorldMatrix));
		auto rgIndexBuffer = passData.GraphBuilder.GetOrAddExternalResource(renderData.Indices);
		auto rgHeightMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.HeightMap);
		auto rgAlbedoMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.TerrainAlbedoTex);
		auto rgNormalMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.TerrainNormalMap);
		auto resources = renderData.Resources;
		resources.MVP = mvp;
		resources.Normal = normalMatrix;
		renderObjects.push_back(RenderObject{
			.IndexCount = renderData.IndexCount,
			.IndexBuffer = pass.AddInput(
				"IdxBuffer", rgIndexBuffer, RGResourceUsage::IndexBufferView(rgIndexBuffer, DXGI_FORMAT_R32_UINT)),
			.HeightMap = pass.AddInput("HeightMap", rgHeightMap, RGResourceUsage::PixelShaderResourceView(rgHeightMap)),
			.AlbedoMap = pass.AddInput("AlbedoMap", rgAlbedoMap, RGResourceUsage::PixelShaderResourceView(rgAlbedoMap)),
			.NormalMap = pass.AddInput("NormalMap", rgNormalMap, RGResourceUsage::PixelShaderResourceView(rgNormalMap)),
			.Resources = resources});
	}

	pass.Execute = [this, renderObjects = std::move(renderObjects)](CommandContext& cmd) {
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		TerrainDepthOnlyPSO.Bind(cmd);

		const RenderObject* lastRenderObj{};
		for (auto& renderObj : renderObjects)
		{
			if (!lastRenderObj || lastRenderObj->IndexBuffer != renderObj.IndexBuffer)
			{
				cmd->IASetIndexBuffer(&renderObj.IndexBuffer->GetResourceView().AsCPUDescriptor<IndexBufferViewDesc>());
			}
			lastRenderObj = &renderObj;
			rad::hlsl::TerrainRenderResources renderResources = renderObj.Resources;
			renderResources.HeightMapTextureIndex =
				renderObj.HeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.TerrainAlbedoTextureIndex =
				renderObj.AlbedoMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.TerrainNormalMapTextureIndex =
				renderObj.NormalMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			TerrainDepthOnlyPSO.SetResources(cmd, renderResources);
			cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
		}
	};
}

void TerrainErosionSystem::TerrainDeferredPass(DeferredPassData& passData)
{
	struct RenderObject
	{
		uint32_t IndexCount;
		Ref<RGBInputResource> IndexBuffer;
		Ref<RGBInputResource> HeightMap;
		Ref<RGBInputResource> AlbedoMap;
		Ref<RGBInputResource> NormalMap;
		hlsl::TerrainRenderResources Resources;
	};

	auto& pass = passData.GraphBuilder.AddPass("TerrainShadowMapPass");

	std::vector<RenderObject> renderObjects;
	renderObjects.reserve(FrameTerrainRenderData.size());
	for (auto& renderData : FrameTerrainRenderData)
	{
		glm::mat4 mvp = passData.Frame.View.ViewProjectionMatrix * renderData.WorldMatrix;
		glm::mat4 normalMatrix = glm::transpose(glm::inverse(renderData.WorldMatrix));
		auto rgIndexBuffer = passData.GraphBuilder.GetOrAddExternalResource(renderData.Indices);
		auto rgHeightMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.HeightMap);
		auto rgAlbedoMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.TerrainAlbedoTex);
		auto rgNormalMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.TerrainNormalMap);
		auto resources = renderData.Resources;
		resources.MVP = mvp;
		resources.Normal = normalMatrix;
		renderObjects.push_back(RenderObject{
			.IndexCount = renderData.IndexCount,
			.IndexBuffer = pass.AddInput(
				"IdxBuffer", rgIndexBuffer, RGResourceUsage::IndexBufferView(rgIndexBuffer, DXGI_FORMAT_R32_UINT)),
			.HeightMap = pass.AddInput("HeightMap", rgHeightMap, RGResourceUsage::PixelShaderResourceView(rgHeightMap)),
			.AlbedoMap = pass.AddInput("AlbedoMap", rgAlbedoMap, RGResourceUsage::PixelShaderResourceView(rgAlbedoMap)),
			.NormalMap = pass.AddInput("NormalMap", rgNormalMap, RGResourceUsage::PixelShaderResourceView(rgNormalMap)),
			.Resources = resources});
	}

	pass.Execute = [this, renderObjects = std::move(renderObjects)](CommandContext& cmd) {
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		TerrainDeferredPSO.Bind(cmd);

		const RenderObject* lastRenderObj{};
		for (auto& renderObj : renderObjects)
		{
			if (!lastRenderObj || lastRenderObj->IndexBuffer != renderObj.IndexBuffer)
			{
				cmd->IASetIndexBuffer(&renderObj.IndexBuffer->GetResourceView().AsCPUDescriptor<IndexBufferViewDesc>());
			}
			lastRenderObj = &renderObj;
			rad::hlsl::TerrainRenderResources renderResources = renderObj.Resources;
			renderResources.HeightMapTextureIndex =
				renderObj.HeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.TerrainAlbedoTextureIndex =
				renderObj.AlbedoMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.TerrainNormalMapTextureIndex =
				renderObj.NormalMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			TerrainDeferredPSO.SetResources(cmd, renderResources);
			cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
		}
	};

	/* auto& cmd = passData.CmdContext;
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
	}*/
}

void TerrainErosionSystem::WaterPass(WaterPassData& passData)
{
	struct RenderObject
	{
		uint32_t IndexCount;
		Ref<RGBInputResource> IndexBuffer;
		Ref<RGBInputResource> HeightMap;
		Ref<RGBInputResource> WaterHeightMap;
		Ref<RGBInputResource> AlbedoMap;
		Ref<RGBInputResource> NormalMap;
		hlsl::WaterRenderResources Resources;
	};

	auto& pass = passData.GraphBuilder.AddPass("WaterPass");

	std::vector<RenderObject> renderObjects;
	renderObjects.reserve(FrameWaterRenderData.size());
	for (auto& renderData : FrameWaterRenderData)
	{
		glm::mat4 mvp = passData.Frame.LightInfo.View.ViewProjectionMatrix * renderData.WorldMatrix;
		glm::mat4 normalMatrix = glm::transpose(glm::inverse(renderData.WorldMatrix));
		auto rgIndexBuffer = passData.GraphBuilder.GetOrAddExternalResource(renderData.Indices);
		auto rgHeightMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.HeightMap);
		auto rgWaterHeightMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.WaterHeightMap);
		auto rgAlbedoMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.WaterAlbedoMap);
		auto rgNormalMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.WaterNormalMap);
		auto resources = renderData.Resources;
		resources.ModelMatrix = renderData.WorldMatrix;
		resources.MVP = mvp;
		resources.Normal = normalMatrix;
		renderObjects.push_back(RenderObject{
			.IndexCount = renderData.IndexCount,
			.IndexBuffer = pass.AddInput(
				"IdxBuffer", rgIndexBuffer, RGResourceUsage::IndexBufferView(rgIndexBuffer, DXGI_FORMAT_R32_UINT)),
			.HeightMap = pass.AddInput("HeightMap", rgHeightMap, RGResourceUsage::PixelShaderResourceView(rgHeightMap)),
			.WaterHeightMap = pass.AddInput(
				"WaterHeightMap", rgWaterHeightMap, RGResourceUsage::PixelShaderResourceView(rgWaterHeightMap)),
			.AlbedoMap = pass.AddInput("AlbedoMap", rgAlbedoMap, RGResourceUsage::PixelShaderResourceView(rgAlbedoMap)),
			.NormalMap = pass.AddInput("NormalMap", rgNormalMap, RGResourceUsage::PixelShaderResourceView(rgNormalMap)),
			.Resources = resources});
	}

	auto rgInViewTransform = pass.AddInput(
		"InViewTransform", passData.InViewTransform, RGResourceUsage::ConstantBufferView(*passData.InViewTransform));

	pass.Execute = [this, renderObjects = std::move(renderObjects), rgInViewTransform](CommandContext& cmd) {
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		WaterPrePassPSO.Bind(cmd);

		const RenderObject* lastRenderObj{};
		for (auto& renderObj : renderObjects)
		{
			if (!lastRenderObj || lastRenderObj->IndexBuffer != renderObj.IndexBuffer)
			{
				cmd->IASetIndexBuffer(&renderObj.IndexBuffer->GetResourceView().AsCPUDescriptor<IndexBufferViewDesc>());
			}
			lastRenderObj = &renderObj;
			auto renderResources = renderObj.Resources;
			renderResources.HeightMapTextureIndex =
				renderObj.HeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.WaterHeightMapTextureIndex =
				renderObj.WaterHeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.WaterAlbedoTextureIndex =
				renderObj.AlbedoMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.WaterNormalMapTextureIndex =
				renderObj.NormalMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.ViewTransformBufferIndex =
				rgInViewTransform->GetResourceView().AsGPUDescriptor<ConstantBufferViewDesc>().Index;

			WaterPrePassPSO.SetResources(cmd, renderResources);
			cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
		}
	};
}

void TerrainErosionSystem::WaterForwardPass(ForwardPassData& passData)
{

	struct RenderObject
	{
		uint32_t IndexCount;
		Ref<RGBInputResource> IndexBuffer;
		Ref<RGBInputResource> HeightMap;
		Ref<RGBInputResource> WaterHeightMap;
		Ref<RGBInputResource> AlbedoMap;
		Ref<RGBInputResource> NormalMap;
		hlsl::WaterRenderResources Resources;
	};

	auto& pass = passData.GraphBuilder.AddPass("WaterPass");

	std::vector<RenderObject> renderObjects;
	renderObjects.reserve(FrameWaterRenderData.size());
	for (auto& renderData : FrameWaterRenderData)
	{
		glm::mat4 mvp = passData.Frame.LightInfo.View.ViewProjectionMatrix * renderData.WorldMatrix;
		glm::mat4 normalMatrix = glm::transpose(glm::inverse(renderData.WorldMatrix));
		auto rgIndexBuffer = passData.GraphBuilder.GetOrAddExternalResource(renderData.Indices);
		auto rgHeightMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.HeightMap);
		auto rgWaterHeightMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.WaterHeightMap);
		auto rgAlbedoMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.WaterAlbedoMap);
		auto rgNormalMap = passData.GraphBuilder.GetOrAddExternalResource(renderData.WaterNormalMap);
		auto resources = renderData.Resources;
		resources.ModelMatrix = renderData.WorldMatrix;
		resources.MVP = mvp;
		resources.Normal = normalMatrix;
		renderObjects.push_back(RenderObject{
			.IndexCount = renderData.IndexCount,
			.IndexBuffer = pass.AddInput(
				"IdxBuffer", rgIndexBuffer, RGResourceUsage::IndexBufferView(rgIndexBuffer, DXGI_FORMAT_R32_UINT)),
			.HeightMap = pass.AddInput("HeightMap", rgHeightMap, RGResourceUsage::PixelShaderResourceView(rgHeightMap)),
			.WaterHeightMap = pass.AddInput(
				"WaterHeightMap", rgWaterHeightMap, RGResourceUsage::PixelShaderResourceView(rgWaterHeightMap)),
			.AlbedoMap = pass.AddInput("AlbedoMap", rgAlbedoMap, RGResourceUsage::PixelShaderResourceView(rgAlbedoMap)),
			.NormalMap = pass.AddInput("NormalMap", rgNormalMap, RGResourceUsage::PixelShaderResourceView(rgNormalMap)),
			.Resources = resources});
	}

	auto rgInViewTransform = pass.AddInput(
		"InViewTransform", passData.InViewTransform, RGResourceUsage::ConstantBufferView(*passData.InViewTransform));

	auto rgInReflectionResultTexture =
		pass.AddInput("InReflectionResultTexture",
					  passData.InReflectionResult,
					  RGResourceUsage::PixelShaderResourceView(*passData.InReflectionResult));
	auto rgInRefractionResultTexture =
		pass.AddInput("InRefractionResultTexture",
					  passData.InRefractionResult,
					  RGResourceUsage::PixelShaderResourceView(*passData.InRefractionResult));
	auto rgInColorTexture = pass.AddInput(
		"InColorTexture", passData.InOutColor, RGResourceUsage::PixelShaderResourceView(*passData.InOutColor));
	auto rgInOpaqueDepthTexture = pass.AddInput(
		"InOpaqueDepthTexture", passData.InOpaquaDepth, RGResourceUsage::PixelShaderResourceView(*passData.InOutColor));

	pass.Execute = [this,
					renderObjects = std::move(renderObjects),
					rgInViewTransform,
					rgInReflectionResultTexture,
					rgInRefractionResultTexture,
					rgInColorTexture,
					rgInOpaqueDepthTexture](CommandContext& cmd) {
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		WaterForwardPSO.Bind(cmd);

		const RenderObject* lastRenderObj{};
		for (auto& renderObj : renderObjects)
		{
			if (!lastRenderObj || lastRenderObj->IndexBuffer != renderObj.IndexBuffer)
			{
				cmd->IASetIndexBuffer(&renderObj.IndexBuffer->GetResourceView().AsCPUDescriptor<IndexBufferViewDesc>());
			}
			lastRenderObj = &renderObj;
			auto renderResources = renderObj.Resources;
			renderResources.HeightMapTextureIndex =
				renderObj.HeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.WaterHeightMapTextureIndex =
				renderObj.WaterHeightMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.WaterAlbedoTextureIndex =
				renderObj.AlbedoMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.WaterNormalMapTextureIndex =
				renderObj.NormalMap->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.ViewTransformBufferIndex =
				rgInViewTransform->GetResourceView().AsGPUDescriptor<ConstantBufferViewDesc>().Index;
			renderResources.ReflectionResultTextureIndex =
				rgInReflectionResultTexture->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.RefractionResultTextureIndex =
				rgInRefractionResultTexture->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.ColorTextureIndex =
				rgInColorTexture->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;
			renderResources.DepthTextureIndex =
				rgInOpaqueDepthTexture->GetResourceView().AsGPUDescriptor<ShaderResourceViewDesc>().Index;

			WaterForwardPSO.SetResources(cmd, renderResources);
			cmd->DrawIndexedInstanced(renderObj.IndexCount, 1, 0, 0, 0);
		}
	};
}
} // namespace rad::proc
