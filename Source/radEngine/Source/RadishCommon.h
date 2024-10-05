#pragma once

#include <string>
#include <locale>
#include <codecvt>
#include <memory>
#include <cassert>
#include <stack>
#include <bit>

namespace rad
{

//Forward declarations
struct DXResource;
struct DXTexture;
struct DXBuffer;
struct DXPipelineState;
struct DXRootSignature;
struct DXDescriptorHeap;

inline std::wstring s2ws(std::string_view str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;
	return converterX.from_bytes(str.data(), str.data() + str.size());
}

inline std::string ws2s(std::wstring_view wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr.data(), wstr.data() + wstr.size());
}

template <typename T, typename... Rest>
void HashCombine(std::size_t& seed, const T& v, const Rest&... rest)
{
	seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
	(HashCombine(seed, rest), ...);
}

template<typename T>
struct Singleton
{
	static void Create()
	{
		if (Instance == nullptr)
			Instance = std::make_unique<T>();
		else
		{
			assert(false);
		}
	}

	static void Destroy()
	{
		if (Instance != nullptr)
			Instance.reset();
	}

	static T& Get()
	{
		assert(Instance);
		return *Instance;
	}

	static std::unique_ptr<T> Instance;
};

}

#define RAD_ID_STRUCT_U32(name_space, name) \
namespace name_space { struct name { uint32_t Id; bool operator==(name const& other) const { return Id == other.Id;}};} \
namespace std { template<> struct hash<name_space::name> { size_t operator()(name_space::name const& id) const { return hash<uint32_t>()(id.Id); }}; }