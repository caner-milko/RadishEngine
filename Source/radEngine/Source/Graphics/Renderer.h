#pragma once

#include "DXResource.h"
#include "EngineCommon.h"
#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{
struct DeferredRenderingPipeline;
struct BlitPipeline;
struct RenderGraphBuilder;

struct RenderView
{
	// TODO RenderGraph: Width and Height should not be here, they are strictly related to rendering,
	// while view settings are more general
	uint32_t Width;
	uint32_t Height;
	// Light data, camera data, etc.
	glm::mat4 ViewProjectionMatrix;
	glm::mat4 ViewMatrix;
	glm::mat4 ProjectionMatrix;
	glm::vec3 ViewPosition;
	glm::vec3 ViewDirection;
	float NearPlane;
	float FarPlane;
};

struct RenderLightInfo
{
	RenderView View{};
	glm::vec3 Color{};
	float Intensity{};
	glm::vec3 AmbientColor{};
};

struct SceneRenderData
{
	uint64_t FrameNumber;
	float DeltaTime;
	RenderView View;
	RenderLightInfo LightInfo;
};

struct Swapchain
{
	uint32_t RequestedNumberOfBackBuffers = 3;
	HANDLE SwapChainWaitableObject = nullptr;
	ComPtr<IDXGISwapChain3> Swapchain;
	std::vector<std::pair<DXTexture, Ref<ResourcePool::ExternalResource>>> BackBuffers;
	DescriptorAllocation BackBufferRTVs;
	DescriptorAllocation BackBufferRGBRTVs;
};

struct PreRenderingGraphBuilder
{
	RenderGraphBuilder& GraphBuilder;
};

/*
Pre Rendering:
Simulate terrain erosion
Rendering:
	ShadowMap Pass
	G-Buffer Pass
	Water Pass
	Screen Space Reflection/Refraction Pass
	Lighting
	Forward Rendering Pass
	Post Processing Pass
	Present
*/

/*
Ideally, seperate device creation, command queue/list creation, and swapchain creation into seperate structs.
*/
struct Renderer
{
	Renderer();
	~Renderer();
	bool Initialize(HWND window, uint32_t width, uint32_t height);
	bool OnWindowResized(uint32_t width, uint32_t height, bool initial = false);

	bool Deinitialize();

	RadDevice& GetDevice() { return *Device.Get(); }

	struct CommandContextData
	{
		Ref<RadDevice> Device;
		ComPtr<ID3D12CommandAllocator> CommandAllocator;
		std::unordered_map<D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorHeapPage*> GPUHeapPages = {};
		std::vector<ComPtr<ID3D12Resource>> IntermediateResources;
	};
	struct ActiveCommandContext
	{
		Ref<RadGraphicsCommandList> CommandList;
		Ref<CommandContextData> CmdContext;
		bool Executed = false;
		CommandContext AsCommandContext()
		{
			return CommandContext{
				CmdContext->Device, CommandList, CmdContext->GPUHeapPages, CmdContext->IntermediateResources};
		}
	};
	struct PendingCommandContext
	{
		Ref<CommandContextData> CmdContext;
		Ref<DXFence> Fence;
		uint64_t FenceValue;
	};

	uint64_t CurrentFrameNumber = 1;

	void RenderScene(SceneRenderData sceneData);
	void FrameIndependentCommand(std::move_only_function<void(CommandContext&)> command);
	void SubmitFrameIndependentCommands(Ref<DXFence> fence, uint64_t signalValue, bool wait);

	void WaitAllCommandContexts();

	ComPtr<RadDevice> Device;
	ComPtr<ID3D12CommandQueue> CommandQueue;
	ComPtr<RadGraphicsCommandList> CommandList;

	std::vector<std::unique_ptr<CommandContextData>> CommandContexts;

	std::deque<Ref<CommandContextData>> AvailableCommandContexts;
	std::deque<PendingCommandContext> PendingCommandContexts;

	std::optional<ActiveCommandContext> FrameIndependentCommandContext = std::nullopt;

	uint32_t BackBufferCount = 3;
	uint32_t FramesInFlight = 3;

	// TODO: Move to a separate class
	std::unique_ptr<ShaderManager> ShaderManager;
	std::unique_ptr<TextureManager> TextureManager;
	std::unique_ptr<ModelManager> ModelManager;

	std::unique_ptr<ResourcePool> ResourcePool;

	DXFence Fence;
	UINT64 FenceLastSignaledValue = 0;

	std::unique_ptr<DeferredRenderingPipeline> DeferredPipeline;
	std::unique_ptr<BlitPipeline> BlitPipeline;
	std::unordered_map<std::string, std::pair<Ref<DXTexture>, DescriptorAllocationView>> ViewableTextures;
	std::optional<std::string> ViewingTexture = std::nullopt;

private:
	std::optional<CommandContextData> CreateCommandContext();
	Swapchain Swapchain;

	bool InitializeDevice();
	bool InitializeResourcePool();
	bool InitializeSwapchain(HWND window, uint32_t width, uint32_t height);
	bool InitializePipelines();

	std::optional<ActiveCommandContext> GetNewCommandContext();
	void ExecuteCommandContext(ActiveCommandContext& context);
	std::optional<PendingCommandContext> SubmitCommandContext(ActiveCommandContext&& context,
															  Ref<DXFence> fence,
															  uint64_t signalValue,
															  bool wait = false);
	CommandContextData& WaitAndClearCommandContext(PendingCommandContext&& context);
};
} // namespace rad