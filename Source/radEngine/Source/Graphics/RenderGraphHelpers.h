#pragma once

#include "RenderGraph.h"

namespace rad::rghelpers
{
template <typename T>
	requires std::is_trivially_copyable_v<T>
RGBOutputResource& UploadTextureData(RenderGraphBuilder& rgBuilder, RGBOutputResource& resource, std::vector<T> data)
{
	auto& uploadBuf = rgBuilder.AddGraphResource(
		resource.Name + "_UploadBuffer",
		ResourceCreateHelper::Buffer(data.size() * sizeof(T), ResourcePresetFlags::UploadResource));

	auto& uploadPass = rgBuilder.AddPass(resource.Name + "_UploadData");
	auto& inUploadBuf =
		uploadPass.AddInput(uploadBuf.Name, uploadBuf, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_SOURCE));
	auto [inRes, outRes] =
		uploadPass.AddInOutResource(resource.Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	uploadPass.Execute =
		[inUploadBuf = Ref(inUploadBuf), inRes = Ref(inRes), data = std::move(data)](CommandContext& cmd)
	{
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data.data();
		subresourceData.RowPitch = inRes.get()->CreateInfo.Desc.Width * sizeof(T);
		subresourceData.SlicePitch = inRes.get()->CreateInfo.Desc.Height * subresourceData.RowPitch;

		uint64_t res = UpdateSubresources(&cmd.CommandList, &inRes.get()->DXRes, &inUploadBuf.get()->DXRes, 0, 0, 1,
										  &subresourceData);
		assert(res != 0);
	};
	return outRes;
}

template <typename T>
	requires std::is_trivially_copyable_v<T>
RGBOutputResource& UploadBufferData(RenderGraphBuilder& rgBuilder, RGBOutputResource& resource, std::vector<T> data,
									size_t offset = 0)
{
	auto& uploadBuf = rgBuilder.AddGraphResource(
		resource.Name + "_UploadBuffer",
		ResourceCreateHelper::Buffer(data.size() * sizeof(T), ResourcePresetFlags::UploadResource));
	auto& uploadPass = rgBuilder.AddPass(resource.Name + "_UploadData");
	auto& inUploadBuf =
		uploadPass.AddInput(uploadBuf.Name, uploadBuf, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_SOURCE));
	auto [inRes, outRes] =
		uploadPass.AddInOutResource(resource.Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	uploadPass.Execute =
		[inUploadBuf = Ref(inUploadBuf), inRes = Ref(inRes), data = std::move(data), offset](CommandContext& cmd)
	{
		T* uploadData = nullptr;
		inUploadBuf.get()->DXRes->Map(0, nullptr, (void**)&uploadData);
		memcpy(uploadData, data.data(), data.size());
		inUploadBuf.get()->DXRes->Unmap(0, nullptr);
		cmd->CopyBufferRegion(&inRes.get()->DXRes, offset, &inUploadBuf.get()->DXRes, 0, data.size());
	};
	return outRes;
}

RGBOutputResource& ClearUnorderedAccessViewFloat(RenderGraphBuilder& rgBuilder, RGBOutputResource& resource,
												 std::array<float, 4> clearValue)
{
	auto& clearPass = rgBuilder.AddPass(resource.Name + "_ClearUAV");
	auto [inRes, outRes] =
		clearPass.AddInOutResource(resource.Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	auto& gpuDesc = inRes.AddDescriptor(DescriptorCreateHelper::UnorderedAccessView(resource.GetCreateInfo()));
	auto& cpuDesc = inRes.AddDescriptor(
		DescriptorCreateHelper::UnorderedAccessView(resource.GetCreateInfo(), {}, DescriptorCreateType::CPU));
	clearPass.Execute = [inRes = Ref(inRes), gpuDesc = Ref(gpuDesc), cpuDesc = Ref(cpuDesc),
						 clearValue = std::move(clearValue)](CommandContext& cmd)
	{
		cmd->ClearUnorderedAccessViewFloat(gpuDesc->Get().AsGPUDescriptor<UnorderedAccessViewDesc>().GetGPUHandle(),
										   gpuDesc->Get().AsGPUDescriptor<UnorderedAccessViewDesc>().GetCPUHandle(),
										   &inRes.get()->DXRes, clearValue.data(), 0, nullptr);
		g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StaticPage->Top--;
	};
	return outRes;
}

void CopyResource(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& src, Ref<RGBOutputResource>& dst)
{
	auto& copyPass = rgBuilder.AddPass(dst->Name + "_CopyResource");
	auto& passSrc = copyPass.AddInResourceSetOut(src->Name, src, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_SOURCE));
	auto& passDst = copyPass.AddInResourceSetOut(dst->Name, dst, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	copyPass.Execute = [&](CommandContext& cmd) { cmd->CopyResource(&passDst->DXRes, &passSrc->DXRes); };
}
} // namespace rad::rghelpers