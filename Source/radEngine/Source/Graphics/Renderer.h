#pragma once

#include "DXResource.h"
#include "RadishCommon.h"
#include "RendererCommon.h"


namespace rad
{
struct DeferredRenderingPipeline;
struct BlitPipeline;

struct RenderView
{
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

struct DepthOnlyPassData
{
	CommandContext& CmdContext;
	const DXTexture* OutDepth;
};

struct DeferredPassData
{
	CommandContext& CmdContext;
	const DXTexture* OutAlbedo;
	const DXTexture* OutNormal;
	const DXTexture* OutDepth;
};

struct WaterPassData
{
	CommandContext& CmdContext;
	const DXTexture* OutReflectionRefraction;
	const DXTexture* OutDepth;
};

struct ForwardPassData
{
	CommandContext& CmdContext;
	const DXTexture* OutColor;
	const DXTexture* Depth;
};

struct RenderCommand
{
	std::string Name;
	void* Data;
	size_t Size;
	std::function<void(const RenderView& view, DepthOnlyPassData& passData)> DepthOnlyPass;
	std::function<void(const RenderView& view, DeferredPassData& passData)> DeferredPass;
	std::function<void(const RenderView& view, WaterPassData& passData)> WaterPass;
	std::function<void(const RenderView& view, ForwardPassData& passData)> ForwardPass;
	std::move_only_function<void()> Destroy;
};

struct RenderQueue
{
	std::deque<RenderCommand> Commands;
};

template <typename T> struct TypedRenderCommand
{
	std::string Name;
	std::vector<T> Data;
	std::function<void(std::span<T> data, const RenderView& view, DepthOnlyPassData& passData)> DepthOnlyPass = nullptr;
	std::function<void(std::span<T> data, const RenderView& view, DeferredPassData& passData)> DeferredPass = nullptr;
	std::function<void(std::span<T> data, const RenderView& view, WaterPassData& passData)> WaterPass = nullptr;
	std::function<void(std::span<T> data, const RenderView& view, ForwardPassData& passData)> ForwardPass = nullptr;
};

/*
Create a FrameResource struct that is used by pipeline commands to pass resources between each other. It is unique per
frame so multiple frames can be processed in parallel.
*/
// using FramePipelineCommand = std::function<void(CommandContext, RenderFrameRecord&)>;

struct CommandRecord
{
	struct CommandRecordItem
	{
		std::string Name;
		std::function<void(CommandContext&)> Command;
	};
	std::queue<CommandRecordItem> Queue;

	void Push(std::string name, std::function<void(CommandContext&)> command)
	{
		Queue.push({std::move(name), std::move(command)});
	}
};

struct RenderFrameRecord
{
	CommandRecord CommandRecord;
	uint64_t FrameNumber;
	RenderView View;
	RenderLightInfo LightInfo;
	std::deque<RenderCommand> Commands;
	// std::deque<FramePipelineCommand> FramePipelineCommands;

	template <typename T> void Push(TypedRenderCommand<T> command)
	{
		std::span<T> span(command.Data);
		void* dataPtr = command.Data.data();
		size_t size = command.Data.size() * sizeof(T);
		RenderCommand renderCommand{
			.Name = std::move(command.Name),
			.Data = dataPtr,
			.Size = size,
			.Destroy = [vec = std::move(command.Data)]() mutable {},
		};
		if (command.DepthOnlyPass)
			renderCommand.DepthOnlyPass = [span, depthPass = std::move(command.DepthOnlyPass)](
											  const RenderView& view, DepthOnlyPassData& passData)
			{ return depthPass(span, view, passData); };
		if (command.DeferredPass)
			renderCommand.DeferredPass = [span, deferredPass = std::move(command.DeferredPass)](
											 const RenderView& view, DeferredPassData& passData)
			{ return deferredPass(span, view, passData); };
		if (command.WaterPass)
			renderCommand.WaterPass =
				[span, waterPass = std::move(command.WaterPass)](const RenderView& view, WaterPassData& passData)
			{ return waterPass(span, view, passData); };
		if (command.ForwardPass)
			renderCommand.ForwardPass =
				[span, forwardPass = std::move(command.ForwardPass)](const RenderView& view, ForwardPassData& passData)
			{ return forwardPass(span, view, passData); };
		Commands.push_back(std::move(renderCommand));
	}
};

struct Swapchain
{
	uint32_t RequestedNumberOfBackBuffers = 3;
	HANDLE SwapChainWaitableObject = nullptr;
	ComPtr<IDXGISwapChain3> Swapchain;
	std::vector<DXTexture> BackBuffers;
	DescriptorAllocation BackBufferRTVs;
	DescriptorAllocation BackBufferRGBRTVs;
};

/*
Ideally, seperate device creation, command queue/list creation, and swapchain creation into seperate structs.
*/
struct Renderer
{
	Renderer();
	~Renderer();
	bool Initialize(bool debug, HWND window, uint32_t width, uint32_t height);
	bool OnWindowResized(uint32_t width, uint32_t height, bool initial = false);

	bool Deinitialize();

	RadDevice& GetDevice()
	{
		return *Device.Get();
	}

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
		CommandContext AsCommandContext()
		{
			return CommandContext{CmdContext->Device, CommandList, CmdContext->GPUHeapPages,
								  CmdContext->IntermediateResources};
		}
	};
	struct PendingCommandContext
	{
		Ref<CommandContextData> CmdContext;
		Ref<DXFence> Fence;
		uint64_t FenceValue;
	};

	uint64_t CurrentFrameNumber = 0;
	std::optional<RenderFrameRecord> CurrentFrameRecord = std::nullopt;

	RenderFrameRecord BeginFrame();
	void EnqueueFrame(RenderFrameRecord frame);

	void Render(RenderFrameRecord& queue);
	void FrameIndependentCommand(std::move_only_function<void(CommandContext&)> command);
	void SubmitFrameIndependentCommands(Ref<DXFence> fence, uint64_t signalValue, bool wait);

	void RenderPendingFrameRecods()
	{
		while (!PendingFrameRecords.empty())
		{
			auto& queue = PendingFrameRecords.front();
			Render(queue);
			PendingFrameRecords.pop();
		}
	}

	std::optional<ActiveCommandContext> GetNewCommandContext();
	std::optional<PendingCommandContext> SubmitCommandContext(ActiveCommandContext&& context, Ref<DXFence> fence,
															  uint64_t signalValue, bool wait = false);
	CommandContextData& WaitAndClearCommandContext(PendingCommandContext&& context);
	void WaitAllCommandContexts();

	std::queue<RenderFrameRecord> PendingFrameRecords;

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

	DXFence Fence;
	UINT64 FenceLastSignaledValue = 0;

	std::unique_ptr<DeferredRenderingPipeline> DeferredPipeline;
	std::unique_ptr<BlitPipeline> BlitPipeline;
	std::unordered_map<std::string, std::pair<Ref<DXTexture>, DescriptorAllocationView>> ViewableTextures;
	std::optional<std::string> ViewingTexture = std::nullopt;
	std::pair<Ref<DXTexture>, DescriptorAllocationView> GetViewingTexture();

  private:
	std::optional<CommandContextData> CreateCommandContext();
	Swapchain Swapchain;

	bool InitializeDevice(bool debug);
	bool InitializeSwapchain(HWND window, uint32_t width, uint32_t height);
	bool InitializePipelines();
};
} // namespace rad