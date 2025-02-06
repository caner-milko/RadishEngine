#pragma once
#include "Graphics/RendererCommon.h"

namespace rad
{
	
struct RGResourceCreateInfo
{
	/**/
};

struct RGResource
{
	std::string Name;
	RGResourceCreateInfo CreateInfo;
};

struct RGExternalResource
{
	std::string Name;
	Ref<DXResource> Resource;
	D3D12_RESOURCE_STATES InitialState;
};

struct RGResourceView
{
	std::variant<Ref<RGExternalResource>, Ref<RGResource>> Resource;
	bool WriteView;
};

struct RenderPassBuilder
{
	std::vector<RGResourceView> Resources;
	std::function<void(CommandContext&)> Execute;
};

struct RenderGraphBuilder
{
	std::deque<RGExternalResource> ExternalResources;
	std::deque<RGResource> Resources;
	std::deque<RenderPassBuilder> Passes;
	RenderPassBuilder& AddPass(std::string name);
	void Build();
};

};