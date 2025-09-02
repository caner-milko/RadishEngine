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
#include <unordered_set>
#include <array>
#include <mutex>
#include <functional>

namespace rad
{
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
void HashCombineRecursive(std::size_t& seed, const T& v, const Rest&... rest)
{
	seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	(HashCombineRecursive(seed, rest), ...);
}

template <typename... Rest>
size_t HashCombine(const Rest&... rest)
{
	size_t seed = 0;
	HashCombineRecursive(seed, rest...);
	return seed;
}

/*
This is purely for syntactic sugar. It allows you to use the -> operator on a reference_wrapper.
*/
template <typename T>
struct Ref : public std::reference_wrapper<T>
{
	using std::reference_wrapper<T>::reference_wrapper;

	Ref(T& ref) : std::reference_wrapper<T>(ref) {}

	// Define -> operator
	T* operator->() const noexcept { return &this->get(); }
	T* Ptr() const noexcept { return &this->get(); }

	T& operator*() const noexcept { return this->get(); }

	T* operator&() const noexcept { return &this->get(); }

	bool operator==(const Ref<T>& other) const noexcept { return this->Ptr() == other.Ptr(); }
};

template <typename T>
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

template <typename T, typename IdType = uint32_t>
struct Id
{
	IdType Value;
	auto operator<=>(const Id& other) const = default;
	friend std::ostream& operator<<(std::ostream& os, const Id<T, IdType>& id) { return os << id.Value; }
};

} // namespace rad

namespace std
{
template <typename T>
struct hash<rad::Ref<T>>
{
	size_t operator()(rad::Ref<T> const& ref) const { return hash<T*>{}(&ref.get()); }
};
template <typename T>
struct hash<rad::OptionalRef<T>>
{
	size_t operator()(rad::OptionalRef<T> const& resource) const { return hash<T*>{}(resource.Ptr); }
};
template <typename T, typename IdType>
struct hash<rad::Id<T, IdType>>
{
	size_t operator()(const rad::Id<T, IdType>& id) const { return hash<IdType>{}(id.Value); }
};
} // namespace std
