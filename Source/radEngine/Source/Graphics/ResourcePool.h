#pragma once

#include "RendererCommon.h"

namespace rad
{
struct ShaderResourceViewDesc : D3D12_SHADER_RESOURCE_VIEW_DESC
{
	bool operator==(const ShaderResourceViewDesc& Other) const;
	size_t Hash() const;
};

struct UnorderedAccessViewDesc : D3D12_UNORDERED_ACCESS_VIEW_DESC
{
	bool operator==(const UnorderedAccessViewDesc& Other) const;
	size_t Hash() const;
};

struct ConstantBufferViewDesc
{
	uint64_t StartOffset = 0;
	uint32_t SizeInBytes = 0;
	bool operator==(const ConstantBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct RenderTargetViewDesc : D3D12_RENDER_TARGET_VIEW_DESC
{
	bool operator==(const RenderTargetViewDesc& Other) const;
	size_t Hash() const;
};

struct DepthStencilViewDesc : D3D12_DEPTH_STENCIL_VIEW_DESC
{
	bool operator==(const DepthStencilViewDesc& Other) const;
	size_t Hash() const;
};

struct VertexBufferViewDesc
{
	uint64_t StartOffset = 0;
	UINT SizeInBytes;
	UINT StrideInBytes;
	bool operator==(const VertexBufferViewDesc& Other) const;
	size_t Hash() const;
};

struct IndexBufferViewDesc
{
	uint64_t StartOffset = 0;
	UINT SizeInBytes;
	DXGI_FORMAT Format;
	bool operator==(const IndexBufferViewDesc& Other) const;
	size_t Hash() const;
};

using CPUDescriptorDesc = std::variant<ShaderResourceViewDesc,
									   UnorderedAccessViewDesc,
									   ConstantBufferViewDesc,
									   RenderTargetViewDesc,
									   DepthStencilViewDesc,
									   VertexBufferViewDesc,
									   IndexBufferViewDesc>;

using GPUDescriptorDesc = std::variant<ShaderResourceViewDesc, UnorderedAccessViewDesc, ConstantBufferViewDesc>;

struct DescriptorDesc
	: std::variant<CPUDescriptorDesc, GPUDescriptorDesc, std::vector<CPUDescriptorDesc>, std::vector<GPUDescriptorDesc>>
{
	using std::variant<CPUDescriptorDesc,
					   GPUDescriptorDesc,
					   std::vector<CPUDescriptorDesc>,
					   std::vector<GPUDescriptorDesc>>::variant;
	bool operator==(const DescriptorDesc& Other) const;
	size_t Hash() const;
};

using CPUResourceDescriptor = std::variant<DescriptorAllocation, D3D12_VERTEX_BUFFER_VIEW, D3D12_INDEX_BUFFER_VIEW>;

using GPUResourceDescriptor = DescriptorAllocation;

struct ResourceDescriptor : std::variant<CPUResourceDescriptor, GPUResourceDescriptor>
{
	using std::variant<CPUResourceDescriptor, GPUResourceDescriptor>::variant;
	template <typename T>
		requires std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
				 std::is_same_v<T, ConstantBufferViewDesc>
	auto& AsGPUDescriptor()
	{
		return std::get<GPUResourceDescriptor>(*this);
	}

	template <typename T>
		requires std::is_same_v<T, RenderTargetViewDesc> || std::is_same_v<T, DepthStencilViewDesc> ||
				 std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
				 std::is_same_v<T, ConstantBufferViewDesc> || std::is_same_v<T, VertexBufferViewDesc> ||
				 std::is_same_v<T, IndexBufferViewDesc>
	auto& AsCPUDescriptor()
	{
		auto& cpuDesc = std::get<CPUResourceDescriptor>(*this);
		if constexpr (std::is_same_v<T, RenderTargetViewDesc> || std::is_same_v<T, DepthStencilViewDesc> ||
					  std::is_same_v<T, ShaderResourceViewDesc> || std::is_same_v<T, UnorderedAccessViewDesc> ||
					  std::is_same_v<T, ConstantBufferViewDesc>)
			return std::get<DescriptorAllocation>(cpuDesc);
		else if constexpr (std::is_same_v<T, VertexBufferViewDesc>)
			return std::get<D3D12_VERTEX_BUFFER_VIEW>(cpuDesc);
		else if constexpr (std::is_same_v<T, IndexBufferViewDesc>)
			return std::get<D3D12_INDEX_BUFFER_VIEW>(cpuDesc);
	}
};

struct ResourceCreateInfo
{
	D3D12_RESOURCE_DESC Desc;
	D3D12_HEAP_PROPERTIES HeapProps;
	D3D12_HEAP_FLAGS HeapFlags;
	std::optional<std::array<float, 4>> ClearValue = std::nullopt;
	bool operator==(const ResourceCreateInfo& Other) const;
	size_t Hash() const;
};
} // namespace rad

#define RAD_DECLARE_HASH(Type)                                                                                         \
	namespace std                                                                                                      \
	{                                                                                                                  \
	template <>                                                                                                        \
	struct hash<Type>                                                                                                  \
	{                                                                                                                  \
		size_t operator()(const Type& val) const                                                                       \
		{                                                                                                              \
			return val.Hash();                                                                                         \
		}                                                                                                              \
	};                                                                                                                 \
	}
RAD_DECLARE_HASH(rad::ShaderResourceViewDesc)
RAD_DECLARE_HASH(rad::UnorderedAccessViewDesc)
RAD_DECLARE_HASH(rad::ConstantBufferViewDesc)
RAD_DECLARE_HASH(rad::RenderTargetViewDesc)
RAD_DECLARE_HASH(rad::DepthStencilViewDesc)
RAD_DECLARE_HASH(rad::VertexBufferViewDesc)
RAD_DECLARE_HASH(rad::IndexBufferViewDesc)
RAD_DECLARE_HASH(rad::ResourceCreateInfo)
namespace std
{
template <>
struct hash<rad::DescriptorDesc>
{
	size_t operator()(const rad::DescriptorDesc& val) const { return val.Hash(); }
};
} // namespace std

namespace rad
{
struct PoolResourceView;
struct ResourcePool
{
	struct Resource
	{
		ResourceCreateInfo CreateInfo;
		Ref<ID3D12Resource> DXRes;
		D3D12_RESOURCE_STATES State;
		std::unordered_map<DescriptorDesc, ResourceDescriptor> Descriptors;

		Resource(Resource&&) = default;
		Resource& operator=(Resource&&) = default;

	private:
		friend struct ResourcePool;
		Resource(ResourceCreateInfo createInfo, ID3D12Resource& resource, D3D12_RESOURCE_STATES initialState)
			: CreateInfo(createInfo), DXRes(resource), State(initialState)
		{
		}
		Resource(const Resource&) = delete;
		Resource& operator=(const Resource&) = delete;
	};
	struct OwnedResource
	{
		operator Resource&() { return *InfoRef; }
		operator const Resource&() const { return *InfoRef; }
		Resource* operator->() { return &InfoRef; }
		const Resource* operator->() const { return &InfoRef; }
		std::string const& GetName() const
		{
			assert(AcquiredName.has_value());
			return *AcquiredName;
		}
		PoolResourceView AsView();
		Resource& Info() const { return *InfoRef; }
		OwnedResource(OwnedResource&&) = default;
		OwnedResource& operator=(OwnedResource&&) = default;

	private:
		friend struct ResourcePool;
		OwnedResource(ComPtr<ID3D12Resource> resource, Resource& resInfo) : DXRes(resource), InfoRef(resInfo) {}
		OwnedResource(const OwnedResource&) = delete;
		OwnedResource& operator=(const OwnedResource&) = delete;
		ComPtr<ID3D12Resource> DXRes;
		Ref<Resource> InfoRef;
		std::optional<std::string> AcquiredName;
	};
	struct ExternalResource
	{
		operator Resource&() { return *Info; }
		operator const Resource&() const { return *Info; }
		Resource* operator->() { return &Info; }
		const Resource* operator->() const { return &Info; }
		std::string const& GetName() const { return Name; }
		PoolResourceView AsView();
		ExternalResource(ExternalResource&&) = default;
		ExternalResource& operator=(ExternalResource&&) = default;

	private:
		ExternalResource(ID3D12Resource& resource, std::string name, Resource& resInfo)
			: DXRes(resource), Name(std::move(name)), Info(resInfo)
		{
		}
		ExternalResource(const ExternalResource&) = delete;
		ExternalResource& operator=(const ExternalResource&) = delete;
		Ref<ID3D12Resource> DXRes;
		std::string Name;
		Ref<Resource> Info;
		friend struct ResourcePool;
	};
	ResourcePool(RadDevice& device);
	OwnedResource& GetResource(const ResourceCreateInfo& createInfo, std::string acquireName);
	void FreeResource(OwnedResource& resource);
	ExternalResource& AddExternalResource(ID3D12Resource& resource,
										  std::string name,
										  const ResourceCreateInfo& createInfo,
										  D3D12_RESOURCE_STATES initialState);
	void RemoveExternalResource(ExternalResource& resource);
	ResourceDescriptor& GetDescriptor(const PoolResourceView& resource, const DescriptorDesc& desc);

private:
	RadDevice& Device;
	Resource& AddResourceInfo(ID3D12Resource& resource,
							  const ResourceCreateInfo& createInfo,
							  D3D12_RESOURCE_STATES initialState);
	std::unordered_map<ResourceCreateInfo, std::deque<OwnedResource>> OwnedResources;
	std::unordered_map<Ref<ID3D12Resource>, Resource> Resources;
	std::unordered_map<Ref<ID3D12Resource>, ExternalResource> ExternalResources;
	std::unordered_map<ResourceCreateInfo, std::deque<Ref<OwnedResource>>> FreeResources;
};
struct PoolResourceView
{
	operator ResourcePool::Resource&() { return *Info; }
	operator const ResourcePool::Resource&() const { return *Info; }
	ResourcePool::Resource* operator->() { return &Info; }
	const ResourcePool::Resource* operator->() const { return &Info; }
	std::string const& GetName() const { return Name; }
	bool operator==(const PoolResourceView& Other) const { return UnderlyingResource == Other.UnderlyingResource; }
	size_t Hash() const { return HashCombine(UnderlyingResource); }
	PoolResourceView(std::variant<Ref<ResourcePool::OwnedResource>, Ref<ResourcePool::ExternalResource>> resInfo)
		: Info(std::visit(
			  [](auto&& arg) -> ResourcePool::Resource& {
				  return *arg;
			  },
			  resInfo)),
		  Name(std::visit(
			  [](auto&& arg) -> std::string const& {
				  return arg->GetName();
			  },
			  resInfo)),
		  UnderlyingResource(resInfo)
	{
	}

private:
	Ref<ResourcePool::Resource> Info;
	Ref<const std::string> Name;
	std::variant<Ref<ResourcePool::OwnedResource>, Ref<ResourcePool::ExternalResource>> UnderlyingResource;
	friend struct ResourcePool;
};
} // namespace rad
RAD_DECLARE_HASH(rad::PoolResourceView)
namespace rad
{
enum class ResourcePresetFlags : uint32_t
{
	None = 0,
	RenderTarget = 1 << 0,
	DepthStencil = 1 << 1,
	ShaderResource = 1 << 2,
	UnorderedAccess = 1 << 3,
	VertexBuffer = 1 << 4,
	IndexBuffer = 1 << 5,
	ConstantBuffer = 1 << 6,
	MipMaps = 1 << 7,
	MipMappedTexture = ShaderResource | MipMaps,

	UploadResource = 1 << 8,
};
#define RAD_DECLARE_ENUM_BITWISE_OPERATORS(Enum)                                                                       \
	inline Enum operator|(Enum a, Enum b)                                                                              \
	{                                                                                                                  \
		return static_cast<Enum>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));                                 \
	}                                                                                                                  \
	inline Enum operator&(Enum a, Enum b)                                                                              \
	{                                                                                                                  \
		return static_cast<Enum>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));                                 \
	}                                                                                                                  \
	inline Enum operator^(Enum a, Enum b)                                                                              \
	{                                                                                                                  \
		return static_cast<Enum>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));                                 \
	}                                                                                                                  \
	inline Enum operator~(Enum a)                                                                                      \
	{                                                                                                                  \
		return static_cast<Enum>(~static_cast<uint32_t>(a));                                                           \
	}                                                                                                                  \
	inline bool operator!(Enum a)                                                                                      \
	{                                                                                                                  \
		return static_cast<uint32_t>(a) == 0;                                                                          \
	}                                                                                                                  \
	inline Enum& operator|=(Enum& a, Enum b)                                                                           \
	{                                                                                                                  \
		return a = a | b;                                                                                              \
	}                                                                                                                  \
	inline Enum& operator&=(Enum& a, Enum b)                                                                           \
	{                                                                                                                  \
		return a = a & b;                                                                                              \
	}                                                                                                                  \
	inline Enum& operator^=(Enum& a, Enum b)                                                                           \
	{                                                                                                                  \
		return a = a ^ b;                                                                                              \
	}
RAD_DECLARE_ENUM_BITWISE_OPERATORS(ResourcePresetFlags)
struct ResourceCreateHelper
{
	enum class PresetType
	{
		InvalidType = 0,
		Buffer,
		Texture2D,
		Texture2DArray,
		Texture2DCube,
		Texture3D,
	};
	struct BufferDetails
	{
		std::optional<D3D12_HEAP_PROPERTIES> Heap = std::nullopt;
		D3D12_RESOURCE_FLAGS DetailedFlags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_HEAP_FLAGS HeapFlags = D3D12_HEAP_FLAG_NONE;
	};
	struct TextureDetails
	{
		std::optional<D3D12_HEAP_PROPERTIES> Heap = std::nullopt;
		D3D12_RESOURCE_FLAGS DetailedFlags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_HEAP_FLAGS HeapFlags = D3D12_HEAP_FLAG_NONE;
		std::optional<std::array<float, 4>> ClearValue = std::nullopt;
	};
	static D3D12_RESOURCE_FLAGS ToResourceFlags(ResourcePresetFlags flags);
	static D3D12_HEAP_FLAGS ToHeapFlags(ResourcePresetFlags flags, PresetType type);
	static D3D12_HEAP_PROPERTIES ToHeapProps(ResourcePresetFlags flags);

	static ResourceCreateInfo Buffer(uint64_t size, ResourcePresetFlags flags, BufferDetails details = {});
	static ResourceCreateInfo Texture2D(
		uint32_t width, uint32_t height, DXGI_FORMAT format, ResourcePresetFlags flags, TextureDetails details = {});
	static ResourceCreateInfo Texture2DArray(uint32_t width,
											 uint32_t height,
											 uint32_t arraySize,
											 DXGI_FORMAT format,
											 ResourcePresetFlags flags,
											 TextureDetails details = {});
	static ResourceCreateInfo Texture2DCube(
		uint32_t width, uint32_t height, DXGI_FORMAT format, ResourcePresetFlags flags, TextureDetails details = {});
	static ResourceCreateInfo Texture3D(uint32_t width,
										uint32_t height,
										uint32_t depth,
										DXGI_FORMAT format,
										ResourcePresetFlags flags,
										TextureDetails details = {});
};
enum class DescriptorCreateFlags : uint32_t
{
	None = 0,
	SRGB = 1 << 0,
	NO_SRGB = 1 << 1,
	MipMaps = 1 << 1,
};
enum class DescriptorCreateType
{
	CPU,
	GPU,
};
RAD_DECLARE_ENUM_BITWISE_OPERATORS(DescriptorCreateFlags)
struct DescriptorCreateHelper
{
	template <typename T>
	struct Details
	{
		T Desc = {};
		DescriptorCreateFlags Flags;
		union {
			struct
			{
				uint64_t StartOffset = 0;
				uint32_t StrideInBytes = 0;
			} Buffer, VertexBuffer;
			struct
			{
				uint64_t StartOffset = 0;
			} IndexBuffer;
			struct
			{
				bool Array = false;
				bool Cube = false;
				bool MultiSample = false;
			} Texture;
		};
	};
	static DescriptorDesc ShaderResourceView(D3D12_SHADER_RESOURCE_VIEW_DESC desc, DescriptorCreateType type)
	{
		if (type == DescriptorCreateType::CPU)
			return DescriptorDesc(CPUDescriptorDesc{ShaderResourceViewDesc{desc}});
		else
			return DescriptorDesc(GPUDescriptorDesc{ShaderResourceViewDesc{desc}});
	}
	static DescriptorDesc UnorderedAccessView(D3D12_UNORDERED_ACCESS_VIEW_DESC desc, DescriptorCreateType type)
	{
		if (type == DescriptorCreateType::CPU)
			return DescriptorDesc(CPUDescriptorDesc{UnorderedAccessViewDesc{desc}});
		else
			return DescriptorDesc(GPUDescriptorDesc{UnorderedAccessViewDesc{desc}});
	}
	static DescriptorDesc ConstantBufferView(ConstantBufferViewDesc desc, DescriptorCreateType type)
	{
		if (type == DescriptorCreateType::CPU)
			return DescriptorDesc(CPUDescriptorDesc{desc});
		else
			return DescriptorDesc(GPUDescriptorDesc{desc});
	}
	static DescriptorDesc RenderTargetView(D3D12_RENDER_TARGET_VIEW_DESC desc)
	{
		return DescriptorDesc(CPUDescriptorDesc{RenderTargetViewDesc{desc}});
	}
	static DescriptorDesc DepthStencilView(D3D12_DEPTH_STENCIL_VIEW_DESC desc)
	{
		return DescriptorDesc(CPUDescriptorDesc{DepthStencilViewDesc{desc}});
	}
	static DescriptorDesc VertexBufferView(VertexBufferViewDesc desc)
	{
		return DescriptorDesc(CPUDescriptorDesc{desc});
	}
	static DescriptorDesc IndexBufferView(IndexBufferViewDesc desc) { return DescriptorDesc(CPUDescriptorDesc{desc}); }

	static DescriptorDesc ShaderResourceView(ResourceCreateInfo const& createInfo,
											 Details<D3D12_SHADER_RESOURCE_VIEW_DESC> details = {},
											 DescriptorCreateType type = DescriptorCreateType::GPU);
	static DescriptorDesc UnorderedAccessView(ResourceCreateInfo const& createInfo,
											  Details<D3D12_UNORDERED_ACCESS_VIEW_DESC> details = {},
											  DescriptorCreateType type = DescriptorCreateType::GPU);
	static DescriptorDesc ConstantBufferView(ResourceCreateInfo const& resource,
											 Details<ConstantBufferViewDesc> details = {},
											 DescriptorCreateType type = DescriptorCreateType::GPU);
	static DescriptorDesc RenderTargetView(ResourceCreateInfo const& createInfo,
										   Details<D3D12_RENDER_TARGET_VIEW_DESC> details = {});
	static DescriptorDesc DepthStencilView(ResourceCreateInfo const& createInfo,
										   Details<D3D12_DEPTH_STENCIL_VIEW_DESC> details = {});
	static DescriptorDesc VertexBufferView(ResourceCreateInfo const& resource,
										   Details<VertexBufferViewDesc> details = {});
	static DescriptorDesc IndexBufferView(ResourceCreateInfo const& resource,
										  DXGI_FORMAT format,
										  Details<IndexBufferViewDesc> details = {});
};
} // namespace rad