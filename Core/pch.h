#pragma once

#include <stdint.h>

#include <memory>
#include <iostream>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

#include <optional>
#include <set>

#include <fstream>
#include <filesystem>
#include <chrono>

#include "Log.h"

#include <locale>
#include <codecvt>

#ifdef _DEBUG
#define LOG_ENABLE_ASSERTS
#endif

#ifdef LOG_ENABLE_ASSERTS
#define LOG_ASSERT(x, ...) { if(!(x)) { LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define LOG_CORE_ASSERT(x, ...) { if(!(x)) { LOG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define LOG_CORE_FATAL_ASSERT(x, ...) { if(!(x)) { LOG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); std::exception(__VA_ARGS__);} }
#else
#define LOG_ASSERT(x, ...)
#define LOG_CORE_ASSERT(x, ...)
#define LOG_CORE_FATAL_ASSERT(x, ...)  { if(!(x)) { LOG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); std::exception(__VA_ARGS__);} }
#endif


#define ASSERT_SUCCEEDED( hr, ... ) \
        if (FAILED(hr)) { \
            LOG_CORE_ERROR("HRESULT failed in {0} {1} @ ", STRINGIFY_BUILTIN(__FILE__), STRINGIFY_BUILTIN(__LINE__)); \
            LOG_CORE_ERROR("\thr = {0}", hr); \
            __debugbreak(); \
        }

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

namespace Core
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        LOG_CORE_ASSERT(SUCCEEDED(hr), "HRESULT failed");
        if (FAILED(hr))
        {
            LOG_CORE_ERROR("HRESULT failed. [hr = {0}]", hr);
            throw std::exception();
        }
    }

    inline std::string NarrowString(const wchar_t* WideStr)
    {
        std::string NarrowStr;

        const std::ctype<wchar_t>& ctfacet = std::use_facet<std::ctype<wchar_t>>(std::wstringstream().getloc());
        for (auto CurrWChar = WideStr; *CurrWChar != 0; ++CurrWChar)
            NarrowStr.push_back(ctfacet.narrow(*CurrWChar, 0));

        return NarrowStr;
    }

    // Clamp a value between a min and max range.
    template<typename T>
    constexpr const T& clamp(const T& val, const T& min, const T& max)
    {
        return val < min ? min : val > max ? max : val;
    }
}


