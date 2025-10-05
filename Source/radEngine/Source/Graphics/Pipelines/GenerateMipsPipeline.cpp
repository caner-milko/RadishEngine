#include "GenerateMipsPipeline.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/DXResource.h"
#include "Graphics/RenderGraphHelpers.h"
#include "Graphics/ResourcePool.h"

#define A_CPU
#include <ffx_a.h>
#include <ffx_spd.h>

namespace rad
{

struct SpdConstants
{
	int mips;
	int numWorkGroupsPerSlice;
	int workGroupOffset[2];
};

bool GenerateMipsPipeline::Setup()
{
	auto* shader = Renderer.ShaderManager->CompileShader(L"SPDImpl.cs",
														 RAD_ENGINE_SHADERS_DIR L"Compute/SPDImpl.cs.hlsl",
														 ShaderType::Compute,
														 L"main",
														 {{FIDELITYFX_SPD_SHADER_INCLUDE_DIR L""}});
	RootSignatureBuilder rsBuilder;
	rsBuilder.AddConstantBufferView("spdConstants",
									{.ShaderRegister = 0,
									 .Visibility = D3D12_SHADER_VISIBILITY_ALL,
									 .DescFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE});
	rsBuilder.AddDescriptorTable("spdGlobalAtomic",
								 {{CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1)}});
	rsBuilder.AddDescriptorTable("imgDst6",
								 {{CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
															 1,
															 2,
															 0,
															 D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
																 D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE)}});
	rsBuilder.AddDescriptorTable("imgDst",
								 {{CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
															 SPD_MAX_MIP_LEVELS + 1,
															 3,
															 0,
															 D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
																 D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE)}});
	RootSignature = rsBuilder.Build("SPDRS", Renderer.GetDevice(), D3D12_ROOT_SIGNATURE_FLAG_NONE);

	struct PipelineStateStream : PipelineStateStreamBase
	{
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream;

	pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(shader->Blob.Get());

	PipelineState =
		PipelineState::Create("SPDPipelineState", Renderer.GetDevice(), pipelineStateStream, &RootSignature);

	// Create the global counter buffer

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	GlobalCounterBuffer = &Renderer.ResourcePool->GetResource(
		ResourceCreateHelper::Buffer(sizeof(GlobalCounterStruct), ResourcePresetFlags::UnorderedAccess),
		"MipPipelineCounterBuffer");
	return true;
}

void GenerateMipsPipeline::GenerateMips(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& tex)
{
	auto& desc = tex->GetCreateInfo().Desc;
	assert(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	assert(desc.MipLevels == 0);

	varAU2(dispatchThreadGroupCountXY);
	varAU2(workGroupOffset); // needed if Left and Top are not 0,0
	varAU2(numWorkGroupsAndMips);
	varAU4(rectInfo) = initAU4(0, 0, uint32_t(desc.Width), uint32_t(desc.Height)); // left, top, width, height

	SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

	SpdConstants constants{};
	constants.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
	constants.mips = numWorkGroupsAndMips[1];
	constants.workGroupOffset[0] = workGroupOffset[0];
	constants.workGroupOffset[1] = workGroupOffset[1];

	auto spdConstantsCB = rgBuilder.AddGraphResource(
		"SPDConstantsCB", ResourceCreateHelper::Buffer(sizeof(SpdConstants), ResourcePresetFlags::ConstantBuffer));

	rghelpers::UploadConstantBufferData(rgBuilder, spdConstantsCB, constants);

	auto& pass = rgBuilder.AddPass("GenerateMips");
	auto input = pass.AddInResourceSetOut("Texture", tex, RGResourceUsage(D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	auto mipUAVDescriptors = [&]() {
		std::vector<GPUDescriptorDesc> mipUavDescs;

		for (int i = 0; i < SPD_MAX_MIP_LEVELS + 5; i++)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Format = desc.Format;
			uavDesc.Texture2D.MipSlice = constants.mips <= i ? constants.mips : i;
			uavDesc.Texture2D.PlaneSlice = 0;
			mipUavDescs.push_back(UnorderedAccessViewDesc{uavDesc});
		}
		return input->AddDescriptor(mipUavDescs);
	}();

	auto rgGlobalCounterBuffer = rgBuilder.GetOrAddExternalResource(GlobalCounterBuffer->AsView());

	rghelpers::UploadConstantBufferData(rgBuilder, rgGlobalCounterBuffer, GlobalCounterStruct{});

	uint32_t dispatchX = dispatchThreadGroupCountXY[0];
	uint32_t dispatchY = dispatchThreadGroupCountXY[1];
	// TODO: Cubemap
	uint32_t dispatchZ = desc.DepthOrArraySize;

	auto inGlobalCounter = pass.AddInput(
		"GlobalCounter", rgGlobalCounterBuffer, RGResourceUsage::UnorderedAccessView(rgGlobalCounterBuffer));
	auto inSpdConstants =
		pass.AddInput("SPDConstants", spdConstantsCB, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	pass.Execute = [this,
					dispatchX,
					dispatchY,
					dispatchZ,
					spdConstantsCB = inSpdConstants->GetResourceView(),
					globalCounter = inGlobalCounter->GetResourceView(),
					mipUAVDescriptors](CommandContext& commandCtx) {
		// downsample

		commandCtx->SetComputeRootSignature(RootSignature.DXSignature.Get());
		commandCtx->SetPipelineState(PipelineState.DXPipelineState.Get());

		// Bind Descriptor the descriptor sets
		//
		int params = 0;
		commandCtx->SetComputeRootConstantBufferView(params++,
													 spdConstantsCB.GetResource()->DXRes->GetGPUVirtualAddress());

		commandCtx->SetComputeRootDescriptorTable(
			params++, globalCounter.AsGPUDescriptor<UnorderedAccessViewDesc>().GetGPUHandle());
		auto& mipUavs = mipUAVDescriptors->Get().AsGPUDescriptor<UnorderedAccessViewDesc>();

		commandCtx->SetComputeRootDescriptorTable(params++, mipUavs.GetGPUHandle(6));
		// bind UAVs
		commandCtx->SetComputeRootDescriptorTable(params++, mipUavs.GetGPUHandle());
		// Dispatch
		//
		commandCtx->Dispatch(dispatchX, dispatchY, dispatchZ);
	};
}

} // namespace rad