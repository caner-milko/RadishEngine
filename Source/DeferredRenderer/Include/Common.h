#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <wrl/client.h>

namespace dfr
{
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
};