#pragma once

#include "RenderGraph.h"

namespace rad::rghelpers
{
template <typename T>
	requires std::is_trivially_copyable_v<T>
void UploadTextureData(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& resource, std::vector<T> data)
{
	auto uploadBuf = rgBuilder.AddGraphResource(
		resource->Name + "_UploadBuffer",
		ResourceCreateHelper::Buffer(data.size() * sizeof(T), ResourcePresetFlags::UploadResource));

	auto& uploadPass = rgBuilder.AddPass(resource->Name + "_UploadData");
	auto inUploadBuf =
		uploadPass.AddInput(uploadBuf->Name, uploadBuf, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_SOURCE));
	auto inRes =
		uploadPass.AddInResourceSetOut(resource->Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	uploadPass.Execute = [inUploadBuf, inRes, data = std::move(data)](CommandContext& cmd) {
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data.data();
		subresourceData.RowPitch = inRes.get()->CreateInfo.Desc.Width * sizeof(T);
		subresourceData.SlicePitch = inRes.get()->CreateInfo.Desc.Height * subresourceData.RowPitch;

		uint64_t res = UpdateSubresources(
			&cmd.CommandList, &inRes.get()->DXRes, &inUploadBuf.get()->DXRes, 0, 0, 1, &subresourceData);
		assert(res != 0);
	};
}

template <typename T>
	requires std::is_trivially_copyable_v<T>
void UploadBufferData(RenderGraphBuilder& rgBuilder,
					  Ref<RGBOutputResource>& resource,
					  std::vector<T> data,
					  size_t offset = 0)
{
	auto uploadBuf = rgBuilder.AddGraphResource(
		resource->Name + "_UploadBuffer",
		ResourceCreateHelper::Buffer(data.size() * sizeof(T), ResourcePresetFlags::UploadResource));
	auto& uploadPass = rgBuilder.AddPass(resource->Name + "_UploadData");
	auto inUploadBuf =
		uploadPass.AddInput(uploadBuf->Name, uploadBuf, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_SOURCE));
	auto inRes =
		uploadPass.AddInResourceSetOut(resource->Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	uploadPass.Execute = [inUploadBuf, inRes, data = std::move(data), offset](CommandContext& cmd) {
		T* uploadData = nullptr;
		inUploadBuf.get()->DXRes->Map(0, nullptr, (void**)&uploadData);
		memcpy(uploadData, data.data(), data.size());
		inUploadBuf.get()->DXRes->Unmap(0, nullptr);
		cmd->CopyBufferRegion(&inRes.get()->DXRes, offset, &inUploadBuf.get()->DXRes, 0, data.size());
	};
}

void ClearUnorderedAccessViewFloat(RenderGraphBuilder& rgBuilder,
								   Ref<RGBOutputResource>& resource,
								   std::array<float, 4> clearValue)
{
	auto& clearPass = rgBuilder.AddPass(resource->Name + "_ClearUAV");
	auto inRes =
		clearPass.AddInResourceSetOut(resource->Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	auto gpuDesc = inRes->AddDescriptor(DescriptorCreateHelper::UnorderedAccessView(resource->GetCreateInfo()));
	auto cpuDesc = inRes->AddDescriptor(
		DescriptorCreateHelper::UnorderedAccessView(resource->GetCreateInfo(), {}, DescriptorCreateType::CPU));
	clearPass.Execute = [inRes, gpuDesc, cpuDesc, clearValue = std::move(clearValue)](CommandContext& cmd) {
		cmd->ClearUnorderedAccessViewFloat(gpuDesc->Get().AsGPUDescriptor<UnorderedAccessViewDesc>().GetGPUHandle(),
										   gpuDesc->Get().AsGPUDescriptor<UnorderedAccessViewDesc>().GetCPUHandle(),
										   &inRes.get()->DXRes,
										   clearValue.data(),
										   0,
										   nullptr);
		g_CPUDescriptorAllocator->Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StaticPage->Top--;
	};
}

void CopyResource(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& src, Ref<RGBOutputResource>& dst)
{
	auto& copyPass = rgBuilder.AddPass(dst->Name + "_CopyResource");
	auto passSrc = copyPass.AddInResourceSetOut(src->Name, src, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_SOURCE));
	auto passDst = copyPass.AddInResourceSetOut(dst->Name, dst, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	copyPass.Execute = [passDst, passSrc](CommandContext& cmd) {
		cmd->CopyResource(&passDst->GetResourceView()->DXRes, &passSrc->GetResourceView()->DXRes);
	};
}

void ClearRenderTargetView(RenderGraphBuilder& rgBuilder,
						   Ref<RGBOutputResource>& resource,
						   std::array<float, 4> clearColor)
{
	auto& clearPass = rgBuilder.AddPass(resource->Name + "_ClearRTV");
	auto inRes = clearPass.AddInResourceSetOut(resource->Name, resource, RGResourceUsage::RenderTargetView(resource));
	clearPass.Execute = [inRes, clearColor = std::move(clearColor)](CommandContext& cmd) {
		cmd->ClearRenderTargetView(inRes->GetResourceView().AsCPUDescriptor<RenderTargetViewDesc>().GetCPUHandle(),
								   clearColor.data(),
								   0,
								   nullptr);
	};
}

void ClearDepthStencilView(RenderGraphBuilder& rgBuilder,
						   Ref<RGBOutputResource>& resource,
						   std::optional<float> depth,
						   std::optional<uint8_t> stencil)
{
	auto& clearPass = rgBuilder.AddPass(resource->Name + "_ClearDSV");
	auto inRes = clearPass.AddInResourceSetOut(resource->Name, resource, RGResourceUsage::DepthStencilWrite(resource));
	clearPass.Execute = [inRes, depth, stencil](CommandContext& cmd) {
		D3D12_CLEAR_FLAGS flags{};
		if (depth.has_value())
			flags |= D3D12_CLEAR_FLAG_DEPTH;
		if (stencil.has_value())
			flags |= D3D12_CLEAR_FLAG_STENCIL;
		cmd->ClearDepthStencilView(inRes->GetResourceView().AsCPUDescriptor<DepthStencilViewDesc>().GetCPUHandle(),
								   flags,
								   depth.value_or(0.0f),
								   stencil.value_or(0),
								   0,
								   nullptr);
	};
}

template <typename T>
void UploadConstantBufferData(RenderGraphBuilder& rgBuilder, Ref<RGBOutputResource>& resource, T const& data)
{
	auto& pass = rgBuilder.AddPass("UploadConstantBufferData_" + resource->Name);
	auto inRes = pass.AddInResourceSetOut(resource->Name, resource, RGResourceUsage(D3D12_RESOURCE_STATE_COPY_DEST));
	pass.Execute = [inRes, data](CommandContext& cmd) {
		constexpr size_t paramCount = sizeof(T) / sizeof(UINT);
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER params[paramCount] = {};
		for (size_t i = 0; i < paramCount; i++)
		{
			params[i].Dest = inRes->GetResourceView()->DXRes->GetGPUVirtualAddress() + i * sizeof(UINT);
			params[i].Value = reinterpret_cast<const UINT*>(&data)[i];
		}
		cmd->WriteBufferImmediate(paramCount, params, nullptr);
	};
}
} // namespace rad::rghelpers