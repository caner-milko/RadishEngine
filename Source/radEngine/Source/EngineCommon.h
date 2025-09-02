#pragma once

#include <radUtil/Common.h>
#include <radUtil/Event.h>

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define RAD_ENABLE_EXPERIMENTAL 0

namespace rad
{

// Forward declarations
struct DXResource;
struct DXTexture;
struct DXBuffer;
struct DXPipelineState;
struct DXRootSignature;
struct DXDescriptorHeap;

} // namespace rad