#pragma once

#include <string>
#include <locale>
#include <codecvt>
#include <memory>
#include <cassert>
#include <stack>
#include <bit>
#include <optional>
#include <unordered_map>
#include <variant>

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define RAD_ENABLE_EXPERIMENTAL 0

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

/*
This is purely for syntactic sugar. It allows you to use the -> operator on a reference_wrapper.
*/
template<typename T>
struct Ref : public std::reference_wrapper<T>
{
	using std::reference_wrapper<T>::reference_wrapper;

	Ref(T& ref) : std::reference_wrapper<T>(ref) {}

	//Define -> operator
	T* operator->() const noexcept
	{
		return &this->get();
	}
	T* Ptr() const noexcept
	{
		return &this->get();
	}
};

template<typename T>
struct OptionalRef
{
	T* Ptr = nullptr;

	OptionalRef() = default;
	OptionalRef(T& ref) : Ptr(&ref) {}
	OptionalRef(Ref<T> ref) : Ptr(&ref.get()) {}
	OptionalRef(T* ptr) : Ptr(ptr) {}
	OptionalRef(std::optional<T>& opt) : Ptr(opt ? &opt.value() : nullptr) {}
	OptionalRef(std::nullopt_t) : Ptr(nullptr) {}
	operator bool() const { return Ptr != nullptr; }
	T& operator*() const { return *Ptr; }
	T* operator->() const { return Ptr; }
};

}

#define RAD_ID_STRUCT_U32(name_space, name) \
namespace name_space { struct name { uint32_t Id; bool operator==(name const& other) const { return Id == other.Id;}};} \
namespace std { template<> struct hash<name_space::name> { size_t operator()(name_space::name const& id) const { return hash<uint32_t>()(id.Id); }}; }