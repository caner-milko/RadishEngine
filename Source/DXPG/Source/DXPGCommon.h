#pragma once

#include <string>
#include <locale>
#include <codecvt>
#include <memory>
#include <cassert>

namespace dxpg
{

//Forward declarations
struct DXResource;
struct DXTexture;
struct DXBuffer;
struct DXPipelineState;
struct DXRootSignature;
struct DXDescriptorHeap;



inline std::wstring s2ws(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

inline std::string ws2s(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
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

#define DXPG_ID_STRUCT_U32(name_space, name) \
namespace name_space { struct name { uint32_t Id; bool operator==(name const& other) const { return Id == other.Id;}};} \
namespace std { template<> struct hash<name_space::name> { size_t operator()(name_space::name const& id) const { return hash<uint32_t>()(id.Id); }}; }