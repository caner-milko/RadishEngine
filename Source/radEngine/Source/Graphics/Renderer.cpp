#include "Renderer.h"

#include "ShaderManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "Pipelines/DeferredRenderingPipeline.h"
#include "Pipelines/BlitPipeline.h"
#include "imgui_impl_dx12.h"

namespace rad
{
Renderer::Renderer() = default;
Renderer::~Renderer() = default;
bool Renderer::InitializeDevice(bool debug)
{
	// [DEBUG] Enable debug interface
	ComPtr<ID3D12Debug> pdx12Debug = nullptr;
	if (debug)
	{
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
			pdx12Debug->EnableDebugLayer();
	}

	// Create device
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
	if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&Device)) != S_OK)
		return false;

	// [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
	if (pdx12Debug != nullptr)
	{
		ComPtr<ID3D12InfoQueue> pInfoQueue = nullptr;
		Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
	}
#endif

	ShaderManager = std::make_unique<rad::ShaderManager>(*this);
	if(!ShaderManager->Init())
		return false;
	
	g_CPUDescriptorAllocator = CPUDescriptorHeapAllocator::Create(GetDevice());
	g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024);
	g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024);
	g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024);
	g_CPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024);

	g_GPUDescriptorAllocator = GPUDescriptorHeapAllocator::Create(GetDevice());
	g_GPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048 * (8 + FramesInFlight + 1), FramesInFlight + 1, 2048 * 8);
	g_GPUDescriptorAllocator->CreateHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128*(FramesInFlight + 2), FramesInFlight + 1, 128);

	TextureManager = std::make_unique<rad::TextureManager>(*this);
	if(!TextureManager->Init())
		return false;

	ModelManager = std::make_unique<rad::ModelManager>(*this);
	//ModelManager->Init();

	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 1;
		if (Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&CommandQueue)) != S_OK)
			return false;
	}

	for (UINT i = 0; i < FramesInFlight; i++)
		CommandContexts.push_back(std::make_unique<CommandContextData>(*CreateCommandContext()));
	// For frame independent commands
	CommandContexts.push_back(std::make_unique<CommandContextData>(*CreateCommandContext()));

	for (auto& cmdContext : CommandContexts)
		AvailableCommandContexts.push_back(*cmdContext);

	if (Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandContexts[0]->CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList)) != S_OK ||
		CommandList->Close() != S_OK)
		return false;

	if (Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence.Fence)) != S_OK)
		return false;

	Fence.FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (Fence.FenceEvent == nullptr)
		return false;
	return true;
}

bool Renderer::InitializeSwapchain(HWND window, uint32_t width, uint32_t height)
{
	//Allocate RTV, SRGB RTV
	Swapchain.BackBufferRTVs = static_cast<RenderTargetView>(g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BackBufferCount));
	Swapchain.BackBufferRGBRTVs = static_cast<RenderTargetView>(g_CPUDescriptorAllocator->AllocateFromStatic(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BackBufferCount));

	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC1 sd;
	{
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = BackBufferCount;
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Scaling = DXGI_SCALING_STRETCH;
		sd.Stereo = FALSE;
	}
	ComPtr<IDXGIFactory4> dxgiFactory = nullptr;
	ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));
	ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(CommandQueue.Get(), window, &sd, nullptr, nullptr, &swapChain1));
	ThrowIfFailed(swapChain1->QueryInterface(IID_PPV_ARGS(&Swapchain.Swapchain)));
	Swapchain.Swapchain->SetMaximumFrameLatency(BackBufferCount);
	Swapchain.SwapChainWaitableObject = Swapchain.Swapchain->GetFrameLatencyWaitableObject();
	return OnWindowResized(width, height, true);
}

bool Renderer::InitializePipelines()
{
	DeferredPipeline = std::make_unique<DeferredRenderingPipeline>(*this);
	BlitPipeline = std::make_unique<rad::BlitPipeline>(*this);
	return DeferredPipeline->Setup() && BlitPipeline->Setup();
}

bool Renderer::Initialize(bool debug, HWND window, uint32_t width, uint32_t height)
{
	return InitializeDevice(debug) && InitializePipelines() && InitializeSwapchain(window, width, height);
}

bool Renderer::OnWindowResized(uint32_t width, uint32_t height, bool initial)
{
	if (!initial)
	{
		Swapchain.BackBuffers.clear();
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(Swapchain.Swapchain->GetDesc(&swapChainDesc));
		ThrowIfFailed(Swapchain.Swapchain->ResizeBuffers(BackBufferCount, width, height,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
	}
	for (uint32_t i = 0; i < BackBufferCount; i++)
	{
		ComPtr<ID3D12Resource> res;
		Swapchain.Swapchain->GetBuffer(i, IID_PPV_ARGS(&res));
		DXTexture::TextureCreateInfo info = {};
		info.Width = width;
		info.Height = height;
		info.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.MipLevels = 1;

		auto& swapchainTex = Swapchain.BackBuffers.emplace_back(DXTexture::FromExisting(GetDevice(), L"Swapchain_" + std::to_wstring(i), res, info));

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		swapchainTex.CreatePlacedRTV(Swapchain.BackBufferRTVs.GetView(i), &rtvDesc);
		auto srgbDesc = rtvDesc;
		srgbDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		swapchainTex.CreatePlacedRTV(Swapchain.BackBufferRGBRTVs.GetView(i), &srgbDesc);
	}
	return 	DeferredPipeline->OnResize(width, height);
}

bool Renderer::Deinitialize()
{
	WaitAllCommandContexts();
	ModelManager.reset();
	TextureManager.reset();
	ShaderManager.reset();
	Swapchain.BackBuffers.clear();
	CommandContexts.clear();
	CommandQueue = nullptr;
	if (Fence.FenceEvent)
	{
		CloseHandle(Fence.FenceEvent);
		Fence.FenceEvent = nullptr;
	}
	Fence.Fence = nullptr;
	CommandList = nullptr;
	if (Swapchain.Swapchain) { Swapchain.Swapchain->SetFullscreenState(false, nullptr); Swapchain.Swapchain = nullptr; }
	if (Swapchain.SwapChainWaitableObject != nullptr) { CloseHandle(Swapchain.SwapChainWaitableObject); }

	g_CPUDescriptorAllocator = nullptr;
	g_GPUDescriptorAllocator = nullptr;
	Device = nullptr;

#ifdef DX12_ENABLE_DEBUG_LAYER

	if (ComPtr<IDXGIDebug1> pDebug = nullptr; SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
	{
		pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
	}
#endif

	return true;
}

RenderFrameRecord Renderer::BeginFrame()
{
	return RenderFrameRecord{ .FrameNumber = CurrentFrameNumber++ };
}

void Renderer::EnqueueFrame(RenderFrameRecord record)
{
	PendingFrameRecords.push(std::move(record));
}

void Renderer::Render(RenderFrameRecord& record)
{
	auto activeCmdContext = GetNewCommandContext();
	if(!activeCmdContext)
	{
		std::cerr << "No command context available" << std::endl;
		return;
	}
	auto cmdContext = activeCmdContext->AsCommandContext();
	WaitForSingleObject(Swapchain.SwapChainWaitableObject, INFINITE);
	while (!record.CommandRecord.Queue.empty())
	{
		auto& command = record.CommandRecord.Queue.front();
		command.Command(cmdContext);
		record.CommandRecord.Queue.pop();
	}

	DeferredPipeline->ShadowMapPass(cmdContext, record);
	DeferredPipeline->DeferredRenderPass(cmdContext, record);
	DeferredPipeline->LightingPass(cmdContext, record);
	DeferredPipeline->ForwardRenderPass(cmdContext, record);
	auto backbufferIndex = Swapchain.Swapchain->GetCurrentBackBufferIndex();
	auto [viewingTexture, viewingTextureSRV] = GetViewingTexture();
	BlitPipeline->Blit(cmdContext, Swapchain.BackBuffers[backbufferIndex],
		viewingTexture,
		Swapchain.BackBufferRGBRTVs.GetView(backbufferIndex),
		viewingTextureSRV);
	TransitionVec(Swapchain.BackBuffers[backbufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET).Execute(cmdContext);
	auto swapchainRTV = Swapchain.BackBufferRTVs.GetView(backbufferIndex).GetCPUHandle();
	cmdContext->OMSetRenderTargets(1, &swapchainRTV, FALSE, nullptr);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), &cmdContext.CommandList);
	TransitionVec(Swapchain.BackBuffers[backbufferIndex], D3D12_RESOURCE_STATE_PRESENT).Execute(cmdContext);
	SubmitCommandContext(std::move(*activeCmdContext), Fence, record.FrameNumber);
	// Present
	Swapchain.Swapchain->Present(1, 0);

}

void Renderer::FrameIndependentCommand(std::move_only_function<void(CommandContext&)> command)
{
	if (!FrameIndependentCommandContext.has_value())
	{
		FrameIndependentCommandContext = GetNewCommandContext();
		if (!FrameIndependentCommandContext.has_value())
		{
			std::cerr << "No command context available" << std::endl;
			return;
		}
	}
	auto cmdContext = FrameIndependentCommandContext->AsCommandContext();
	command(cmdContext);
}

void Renderer::SubmitFrameIndependentCommands(Ref<DXFence> fence, uint64_t signalValue, bool wait)
{
	if (FrameIndependentCommandContext.has_value())
	{
		SubmitCommandContext(std::move(*FrameIndependentCommandContext), fence, signalValue, wait);
		FrameIndependentCommandContext = std::nullopt;
	}
}
std::optional<Renderer::ActiveCommandContext> Renderer::GetNewCommandContext()
{
	OptionalRef<CommandContextData> cmdContext = std::nullopt;
	if (AvailableCommandContexts.empty())
	{
		cmdContext = WaitAndClearCommandContext(std::move(PendingCommandContexts.front()));
		PendingCommandContexts.pop_front();
	}
	else
	{
		cmdContext = AvailableCommandContexts.front();
	}
	if (!cmdContext)
		return std::nullopt;
	else
		AvailableCommandContexts.pop_front();
	CommandList->Reset(cmdContext->CommandAllocator.Get(), nullptr);
	cmdContext->GPUHeapPages[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = g_GPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->AllocatePage();
	cmdContext->GPUHeapPages[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = g_GPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->AllocatePage();

	auto heaps = g_GPUDescriptorAllocator->GetHeaps(); 
	CommandList->SetDescriptorHeaps(heaps.size(), heaps.data());
	return ActiveCommandContext{ *CommandList.Get(), *cmdContext };
}
std::optional<Renderer::PendingCommandContext> Renderer::SubmitCommandContext(ActiveCommandContext&& context, Ref<DXFence> fence, uint64_t signalValue, bool wait)
{
	context.CommandList->Close();
	ID3D12CommandList* cmdLists[] = { &context.CommandList.get()};
	CommandQueue->ExecuteCommandLists(1, cmdLists);
	CommandQueue->Signal(fence->Fence.Get(), signalValue);
	PendingCommandContext pendingContext{ context.CmdContext, fence, signalValue };
	if (wait)
	{
		WaitAndClearCommandContext(std::move(pendingContext));
		return std::nullopt;
	}
	PendingCommandContexts.push_back(std::move(pendingContext));
	return PendingCommandContexts.back();
}
Renderer::CommandContextData& Renderer::WaitAndClearCommandContext(PendingCommandContext&& pendingContext)
{
	pendingContext.Fence->Fence->SetEventOnCompletion(pendingContext.FenceValue, pendingContext.Fence->FenceEvent);
	WaitForSingleObject(pendingContext.Fence->FenceEvent, INFINITE);
	auto cmdContext = pendingContext.CmdContext;
	for (auto const& [type, heapPage] : cmdContext->GPUHeapPages)
		g_GPUDescriptorAllocator->Heaps[type]->FreePage(heapPage);
	
	cmdContext->GPUHeapPages.clear();
	cmdContext->CommandAllocator->Reset();
	cmdContext->IntermediateResources.clear();

	// Delete from PendingCommandContexts
	AvailableCommandContexts.push_back(cmdContext);
	return cmdContext;
}
void Renderer::WaitAllCommandContexts()
{
	while (!PendingCommandContexts.empty())
	{
		WaitAndClearCommandContext(std::move(PendingCommandContexts.front()));
		PendingCommandContexts.pop_front();
	}
}
std::pair<Ref<DXTexture>, DescriptorAllocationView> Renderer::GetViewingTexture()
{
	if (ViewingTexture)
		if (auto it = ViewableTextures.find(*ViewingTexture); it != ViewableTextures.end())
			return it->second;
	return { DeferredPipeline->GetOutputBuffer(), DeferredPipeline->GetOutputBufferSRV()};
}
std::optional<Renderer::CommandContextData> Renderer::CreateCommandContext()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	if (Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)) != S_OK)
	{
		std::cerr << "Failed to create command allocator" << std::endl;
		return std::nullopt;
	}
	return CommandContextData{ .Device = *Device.Get(),.CommandAllocator = std::move(commandAllocator)};
}
} // namespace rad
