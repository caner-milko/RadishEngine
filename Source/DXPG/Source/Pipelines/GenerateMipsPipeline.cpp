#include "GenerateMipsPipeline.h"

#include "ShaderManager.h"

#define A_CPU
#include <ffx_a.h>
#include <ffx_spd.h>

namespace dxpg
{

struct SpdConstants
{
	int mips;
	int numWorkGroupsPerSlice;
	int workGroupOffset[2];
};

bool GenerateMipsPipeline::Setup(ID3D12Device2* dev)
{
	Device = dev;
	auto* shader = dx12::ShaderManager::Get().CompileShader(L"SPDImpl.cs", DXPG_SHADERS_DIR L"Compute/SPDImpl.cs.hlsl", dx12::ShaderType::Compute, L"main", { {FIDELITYFX_SPD_SHADER_INCLUDE_DIR L""} });
	dx12::RootSignatureBuilder rsBuilder;
	rsBuilder.AddConstantBufferView("spdConstants", 0, D3D12_SHADER_VISIBILITY_ALL);
	rsBuilder.AddDescriptorTable("spdGlobalAtomic", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1)}});
	rsBuilder.AddDescriptorTable("imgDst6", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2)} });
	rsBuilder.AddDescriptorTable("imgDst", { { CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, SPD_MAX_MIP_LEVELS + 1, 3)} });
	RootSignature = rsBuilder.Build("SPDRS", dev, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	struct PipelineStateStream : dx12::PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream;

	pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(shader->Blob.Get());

	PipelineState = dx12::PipelineState::Create("SPDPipelineState", dev, pipelineStateStream, &RootSignature);

	// Create the global counter buffer

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	auto globalCounterDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * 6, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(dev->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&globalCounterDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&GlobalCounterBuffer)));

	dx12::ResourceViewToDesc<dx12::ViewTypes::UnorderedAccessView> uavDescs;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(uint32_t) * 6;

	uavDescs.Desc = &uavDesc;
	uavDescs.Resource = GlobalCounterBuffer.Get();

	GlobalCounterUAV = dx12::UnorderedAccessView::Create(uavDescs);
	return true;
}

void GenerateMipsPipeline::GenerateMips(FrameContext& frameCtx, ID3D12GraphicsCommandList2* cmdList, ID3D12Resource* src, ID3D12Resource** outConstantBuffer, uint32_t width, uint32_t height, uint32_t arraySize)
{
	varAU2(dispatchThreadGroupCountXY);
	varAU2(workGroupOffset); // needed if Left and Top are not 0,0
	varAU2(numWorkGroupsAndMips);
	varAU4(rectInfo) = initAU4(0, 0, width, height); // left, top, width, height
	SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

	// downsample
	uint32_t dispatchX = dispatchThreadGroupCountXY[0];
	uint32_t dispatchY = dispatchThreadGroupCountXY[1];
	uint32_t dispatchZ = arraySize;

	uint32_t* pConstMem;


	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(SpdConstants));

	ThrowIfFailed(Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&cbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(outConstantBuffer)));
	ThrowIfFailed((*outConstantBuffer)->Map(0, nullptr, (void**)&pConstMem));

	SpdConstants constants;
	constants.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
	constants.mips = numWorkGroupsAndMips[1];
	constants.workGroupOffset[0] = workGroupOffset[0];
	constants.workGroupOffset[1] = workGroupOffset[1];
	memcpy(pConstMem, &constants, sizeof(SpdConstants));

	cmdList->SetComputeRootSignature(RootSignature.DXSignature.Get());

	auto mipUavs = frameCtx.GPUHeapPages[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(SPD_MAX_MIP_LEVELS+5);
	
	for (int i = 0; i < SPD_MAX_MIP_LEVELS + 5; i++)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture2D.MipSlice = constants.mips <= i ? constants.mips : i;
		uavDesc.Texture2D.PlaneSlice = 0;
		Device->CreateUnorderedAccessView(src, nullptr, &uavDesc, mipUavs->GetCPUHandle(i));
	}

	// Bind Descriptor the descriptor sets
	//                
	int params = 0;
	cmdList->SetComputeRootConstantBufferView(params++, (*outConstantBuffer)->GetGPUVirtualAddress());
	cmdList->SetComputeRootDescriptorTable(params++, frameCtx.GetGPUAllocation(GlobalCounterUAV.get())->GetGPUHandle());
	cmdList->SetComputeRootDescriptorTable(params++, mipUavs->GetGPUHandle(6));
	// bind UAVs
	cmdList->SetComputeRootDescriptorTable(params++, mipUavs->GetGPUHandle());

	// Bind Pipeline
	//
	cmdList->SetPipelineState(PipelineState.DXPipelineState.Get());

	// set counter to 0
	D3D12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(GlobalCounterBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->ResourceBarrier(1, &resourceBarrier);

	D3D12_WRITEBUFFERIMMEDIATE_PARAMETER pParams[6];
	for (int i = 0; i < 6; i++)
	{
		pParams[i] = { GlobalCounterBuffer.Get()->GetGPUVirtualAddress() + sizeof(uint32_t) * i, 0 };
	}
	cmdList->WriteBufferImmediate(6, pParams, NULL); // 6 counter per slice, each initialized to 0

	D3D12_RESOURCE_BARRIER resourceBarriers[2] = {
		CD3DX12_RESOURCE_BARRIER::Transition(GlobalCounterBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0),
		CD3DX12_RESOURCE_BARRIER::Transition(src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};
	cmdList->ResourceBarrier(2, resourceBarriers);

	// Dispatch
	//
	cmdList->Dispatch(dispatchX, dispatchY, dispatchZ);

	// Transition the resources back
	resourceBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalCounterBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	resourceBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(src, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->ResourceBarrier(2, resourceBarriers);
}

}