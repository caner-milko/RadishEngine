#pragma once

#include "DXResource.h"
#include "RadishCommon.h"
#include "RendererCommon.h"
#include "ResourcePool.h"

namespace rad
{
struct DeferredRenderingPipeline;
struct BlitPipeline;
struct RenderGraphBuilder;

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
	const DescriptorAllocationView InViewTransformCBV;
};

struct ForwardPassData
{
	CommandContext& CmdContext;
	const DXTexture* OutColor;
	const DXTexture* SSDepth;
	const DescriptorAllocationView InViewTransformCBV;
	const DescriptorAllocationView InColorSRV;
	const DescriptorAllocationView InOpaqueDepthSRV;
	const DescriptorAllocationView InReflectionResultSRV;
	const DescriptorAllocationView InRefractionResultSRV;
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

struct PipelineData
{
	std::string Name;
	std::function<void*()> GetData;
};

struct PipelineUserBase
{
	~PipelineUserBase();
	std::vector<Ref<struct PipelineBase>> RegisteredPasses;
	// No need to store it here, it's stored in the PipelineDataHolder, but its a stack & finding the data would be harder, so this is ok too
	PipelineData* Data = nullptr;
};

template<typename T>
struct PipelineUser : PipelineUserBase
{
	auto GetData()
	{
		if constexpr (std::is_same_v<T, void>)
			return Data->GetData();
		else
			return *static_cast<T*>(Data->GetData());
	}
};

struct PipelineBase
{
	template <typename U, bool = std::is_void_v<U>> struct VoidPtrOrRef
	{
		using type = U&;
	};
	template <typename U> struct VoidPtrOrRef<U, true>
	{
		using type = void*;
	};

	PipelineBase(std::string name) : Name(std::move(name)) {}
	virtual ~PipelineBase() = default;
	std::string Name;
	std::unordered_map<Ref<PipelineUserBase>, PipelineData> Data;
	std::vector<Ref<PipelineBase>> SubPipelines;
	bool Recording = false;
	std::vector<Ref<PipelineUserBase>> Users;
	std::vector<std::function<void(void* passData, void* userData)>> Commands;


	virtual PipelineData GetPassData(std::stack<PipelineData>& pipelineDataStack) = 0;
	virtual void Run(std::stack<PipelineData>& pipelineDataStack, std::unordered_map<Ref<PipelineUserBase>, PipelineData>& pipelineUserDatas) = 0;

	template <typename U> 
	void RegisterUser(PipelineUser<U>& user, std::function<void(void* passData, VoidPtrOrRef<U> userData)> command)
	{
		PipelinePassBase::RegisterUser(user, [command = std::move(command)](void* passData, void* userData)
				{ command(passData, *static_cast<U*>(userData)); });
	}

	void DoBeginRecording()
	{
		Recording = true;
		BeginRecording();
	}

	void DoEndRecording()
	{
		EndRecording();
		Recording = false;
	}

	// Maybe return a stack?
	std::unordered_map<Ref<PipelineBase>, decltype(Data)> PopPipelineUserDataStack()
	{
		Recording = false;
		std::unordered_map<Ref<PipelineBase>, decltype(Data)> result;
		for (PipelineBase& subPipeline : SubPipelines)
		{
			auto subRes = subPipeline.PopPipelineUserDataStack();
			result.insert(subRes.begin(), subRes.end());
		}
		result[*this] = std::move(Data);
		Data.clear();
		return result;
	}

	PipelineData& PushPipelineData(PipelineUserBase& user, PipelineData&& data)
	{
		assert(Recording);
		return Data.insert_or_assign(user, std::move(data)).first->second;
	}

	template<typename T> 
	PipelineData& PushPipelineData(PipelineUser<T>& user, std::string name, T data)
	{
		return Push(user, {name, [data = std::move(data)]() { return &data; }});
	}

	void AddSubPipeline(PipelineBase& subPipeline)
	{
		SubPipelines.push_back(subPipeline);
	}

	virtual void UnregisterUser(PipelineUserBase& user)
	{
		auto it = std::find(Users.begin(), Users.end(), user);
		if (it == Users.end())
			return;
		Commands.erase(Commands.begin() + std::distance(Users.begin(), it));
		Users.erase(it);
		std::erase_if(user.RegisteredPasses, [this](const Ref<PipelineBase>& pass) { return &pass == this; });
	}

  protected:
	virtual void BeginRecording() {}
	virtual void EndRecording() {}
	void RegisterUser(PipelineUserBase& user, std::function<void(void* passData, void* userData)> command)
	{
		Users.push_back(user);
		Commands.push_back(std::move(command));
		user.RegisteredPasses.push_back(*this);
	}
};

template<typename T, typename... ParentPipelines> 
struct Pipeline : PipelineBase
{
	using ValueType = T;

	Pipeline(std::string name, ParentPipelines&... parentPipelines)
		: PipelineBase(std::move(name)), ParentPipelines{parentPipelines...}
	{
	}
	PipelineData GetPassData(std::stack<PipelineData>& pipelineDataStack) override
	{
		PipelineData result;
		result.Name = Name;
		result.GetData = [this, &pipelineDataStack]() -> void*
		{
			T data;
			for (auto& parentPipeline : ParentPipelines)
			{
				auto parentData = parentPipeline.GetPassData(pipelineDataStack);
				data.ParentData.push_back(parentData);
			}
			return &data;
		};
		return result;
	}

	virtual void Run(ParentPipelines::ValueType&... parentData,
					 std::unordered_map<Ref<PipelineUserBase>, PipelineData>& pipelineUserDatas) = 0;

	void Run(std::stack<PipelineData>& pipelineDataStack,
			 std::unordered_map<Ref<PipelineUserBase>, PipelineData>& pipelineUserDatas) override
	{
		T data;

		for (auto& parentPipeline : ParentPipelines)
		{
			auto parentData = parentPipeline.GetPassData(pipelineDataStack);
			data.ParentData.push_back(parentData);
		}
		Run(data, pipelineUserDatas);
	}
	std::tuple<ParentPipelines&...> ParentPipelines;
}

PipelineUserBase::~PipelineUserBase()
{
	while (!RegisteredPasses.empty())
	{
		auto pass = RegisteredPasses.back();
		pass->UnregisterUser(*this);
	}
}

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
	std::unordered_map<Ref<PipelineBase>, decltype(PipelineBase::Data)> PipelineDatas;

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
	std::vector<std::pair<DXTexture, Ref<ResourcePool::ExternalResource>>> BackBuffers;
	DescriptorAllocation BackBufferRTVs;
	DescriptorAllocation BackBufferRGBRTVs;
};

struct ComputePipeline : PipelinePass<void>
{

};

struct GraphicsPipeline : PipelinePass<void>
{
	void Run(RenderFrameRecord& frameRec, RenderGraphBuilder& rgBuilder)
	{

	}
};

struct FramePipeline : PipelinePass<void>
{
	ComputePipeline ComputePipeline;
	GraphicsPipeline GraphicsPipeline;

	FramePipeline()
	{
		AddSubPipeline(ComputePipeline);
		AddSubPipeline(GraphicsPipeline);
	}

	void Run(RenderFrameRecord& frameRec, RenderGraphBuilder& rgBuilder)
	{
		PipelinePass::Run(RenderFrameRecord & frameRec, rgBuilder);
	}
};

/*
Ideally, seperate device creation, command queue/list creation, and swapchain creation into seperate structs.
*/
struct Renderer : PipelineDataHolder
{
	Renderer();
	~Renderer();
	bool Initialize(HWND window, uint32_t width, uint32_t height);
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
		bool Executed = false;
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

	uint64_t CurrentFrameNumber = 1;
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
	void ExecuteCommandContext(ActiveCommandContext& context);
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

	std::unique_ptr<ResourcePool> ResourcePool;

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

	bool InitializeDevice();
	bool InitializeResourcePool();
	bool InitializeSwapchain(HWND window, uint32_t width, uint32_t height);
	bool InitializePipelines();
};
} // namespace rad