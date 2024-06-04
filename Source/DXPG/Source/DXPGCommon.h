#pragma once

#include <string>
#include <locale>
#include <codecvt>
namespace dxpg
{
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