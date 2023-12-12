#pragma once

//stl
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <map>
#include <assert.h>
#include <chrono>
#include <array>
#include <string_view>
#include <functional>
#define NOMINMAX
#include <wrl/client.h>

namespace dfr
{
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

template<typename T>
using rc = std::shared_ptr<T>;
template<typename T>
using ru = std::unique_ptr<T>;
template<typename T>
using rb = T*;

using Clock = std::chrono::high_resolution_clock;

inline void ThrowIfFailed(long hr)
{
	if (hr < 0)
	{
		throw std::exception();
	}
}

#define STR1(x) #x
#define STR(x) STR1(x)
#define WSTR1(x) L##x
#define WSTR(x) WSTR1(x)
};