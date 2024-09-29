#include "TerrainGenerator.h"

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
	MapVector(size_t x, size_t y) : std::vector<T>(x * y), X(x), Y(y) {}
	T& operator()(size_t x, size_t y)
	{
		return this->at(GetIndex(x, y, X));
	}
};


//from https://medium.com/@nickobrien/diamond-square-algorithm-explanation-and-c-implementation-5efa891e486f
float random()
{
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
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

std::vector<float> TerrainGenerator::CreateDiamondSquareHeightMap(uint32_t toPowerOfTwo, uint32_t& width)
{
	width = (1 << toPowerOfTwo) + 1;
	MapVector<float> heightMap(width, width);
	heightMap[0] = random();
	heightMap[width - 1] = random();
	heightMap[width * (width - 1)] = random();
	heightMap[width * width - 1] = random();

	diamondSquare(heightMap, width);

#if 0
	//print out the heightmap, all values should be printes with 3 digits
	for (int y = 0; y < width; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int value = heightMap(x, y) * 256;
			if (value < 10)
				std::cout << "  " << value << " ";
			else if (value < 100)
				std::cout << " " << value << " ";
			else
				std::cout << value << " ";
		}
		std::cout << std::endl;
	}
#endif

	return heightMap;
}

TerrainData TerrainGenerator::InitializeTerrain(ID3D12Device* device, uint32_t resX, uint32_t resY)
{
	return { .MeshResX = resX, .MeshResY = resY };
}

struct Mesh
{
	std::vector<DirectX::XMFLOAT3> Vertices;
	std::vector<uint32_t> Indices;
};

float LinearSampleHeightMap(const std::vector<float>& heightMap, float u, float v)
{
	auto width = static_cast<uint32_t>(sqrt(heightMap.size()));
	auto x = static_cast<uint32_t>(std::max(std::min(u, 1.f), 0.f) * (width - 1));
	auto y = static_cast<uint32_t>(std::max(std::min(v, 1.f), 0.f) * (width - 1));
	auto x1 = std::min(x + 1, width - 1);
	auto y1 = std::min(y + 1, width - 1);
	auto x0y0 = heightMap[GetIndex(x, y, width)];
	auto x1y0 = heightMap[GetIndex(x1, y, width)];
	auto x0y1 = heightMap[GetIndex(x, y1, width)];
	auto x1y1 = heightMap[GetIndex(x1, y1, width)];
	auto u0 = u * width - x;
	auto v0 = v * width - y;
	auto u1 = 1 - u0;
	auto v1 = 1 - v0;
	return x0y0 * u1 * v1 + x1y0 * u0 * v1 + x0y1 * u1 * v0 + x1y1 * u0 * v0;
}

Vertex ToVertex(TerrainData& data, std::vector<float>& heightMapVals, uint32_t x, uint32_t y)
{
	float texelSize = 1.0f / data.HeightMap.Info.Width;

	float u = x / (float)data.MeshResX;
	float v = y / (float)data.MeshResY;

	float height = LinearSampleHeightMap(heightMapVals, u, v);
	// Calculate normal
	float heightLeft = LinearSampleHeightMap(heightMapVals, u - texelSize, v);
	float heightRight = LinearSampleHeightMap(heightMapVals, u + texelSize, v);
	float heightDown = LinearSampleHeightMap(heightMapVals, u, v - texelSize);
	float heightUp = LinearSampleHeightMap(heightMapVals, u, v + texelSize);

	float xDif = (heightLeft - heightRight) / (2.0f * texelSize);
	float yDif = (heightDown - heightUp) / (2.0f * texelSize);

	DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVectorSet(xDif, 2.0f, yDif, 0.0f));
	
	DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, -yDif, 2.0f * texelSize, 0.f));
	Vertex vtx{
		.Position = { u, height, v },
		.TexCoord = {u, v}
	};
	DirectX::XMStoreFloat3(&vtx.Normal, normal);
	DirectX::XMStoreFloat3(&vtx.Tangent, tangent);
	return vtx;
};

void TerrainGenerator::GenerateBaseHeightMap(ID3D12Device* device, FrameContext& frameCtx, ID3D12GraphicsCommandList* cmdList, TerrainData& terrain, uint32_t toPowerOfTwo, bool createMesh)
{
	//create heightmap
	uint32_t width;
	auto heightMapVals = CreateDiamondSquareHeightMap(8, width);
	auto& heightMap = terrain.HeightMap = DXTexture::Create(device, L"HeightMap", DXTexture::TextureCreateInfo
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Width = width,
			.Height = width,
			.Format = DXGI_FORMAT_R32_FLOAT,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		});
	heightMap.UploadDataTyped<float>(frameCtx, cmdList, heightMapVals);

	if (createMesh)
	{
		std::vector<Vertex> vertices(width * width);
		for (uint32_t y = 0; y < width; y++)
			for (uint32_t x = 0; x < width; x++)
				vertices[GetIndex(x, y, width)] = ToVertex(terrain, heightMapVals, x, y);
		std::vector<uint32_t> indices((width - 1) * (width - 1) * 6);
		for (uint32_t y = 0; y < width - 1; y++)
			for (uint32_t x = 0; x < width - 1; x++)
			{
				auto vtx1 = GetIndex(x, y, width);
				auto vtx2 = GetIndex(x + 1, y, width);
				auto vtx3 = GetIndex(x, y + 1, width);
				auto vtx4 = GetIndex(x + 1, y + 1, width);
				indices[6 * GetIndex(x, y, width - 1) + 0] = vtx1;
				indices[6 * GetIndex(x, y, width - 1) + 1] = vtx3;
				indices[6 * GetIndex(x, y, width - 1) + 2] = vtx2;
				indices[6 * GetIndex(x, y, width - 1) + 3] = vtx3;
				indices[6 * GetIndex(x, y, width - 1) + 4] = vtx4;
				indices[6 * GetIndex(x, y, width - 1) + 5] = vtx2;
			}
		terrain.Model.Vertices.VerticesBuffer = DXTypedBuffer<Vertex>::CreateAndUpload(device, L"TerrainVtxBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), vertices);
		auto& vtxBufView = terrain.Model.Vertices.VertexBufferView;
		vtxBufView.BufferLocation = terrain.Model.Vertices.VerticesBuffer.Resource->GetGPUVirtualAddress();
		vtxBufView.SizeInBytes = terrain.Model.Vertices.VerticesBuffer.Size;
		vtxBufView.StrideInBytes = sizeof(Vertex);

		terrain.Model.Indices = DXTypedBuffer<uint32_t>::CreateAndUpload(device, L"TerrainIdxBuffer", cmdList, frameCtx.IntermediateResources.emplace_back(), indices);
		auto& idxBufView = terrain.Model.IndexBufferView;
		idxBufView.BufferLocation = terrain.Model.Indices.Resource->GetGPUVirtualAddress();
		idxBufView.SizeInBytes = terrain.Model.Indices.Size;
		idxBufView.Format = DXGI_FORMAT_R32_UINT;
	}
}

}