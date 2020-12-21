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

#include <wrl.h>

#include <d3d12.h>

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

    inline void SetD3DDebugName(Microsoft::WRL::ComPtr<ID3D12Object> d3dObject, std::wstring const& name)
    {
#ifdef DEBUG
        ThrowIfFailed(
            d3dObject->SetName(name.c_str()));
#endif
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



// Hashers for view descriptions.
namespace std
{
    // Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
    template <typename T>
    inline void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<>
    struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC>
    {
        std::size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc) const noexcept
        {
            std::size_t seed = 0;

            hash_combine(seed, srvDesc.Format);
            hash_combine(seed, srvDesc.ViewDimension);
            hash_combine(seed, srvDesc.Shader4ComponentMapping);

            switch (srvDesc.ViewDimension)
            {
            case D3D12_SRV_DIMENSION_BUFFER:
                hash_combine(seed, srvDesc.Buffer.FirstElement);
                hash_combine(seed, srvDesc.Buffer.NumElements);
                hash_combine(seed, srvDesc.Buffer.StructureByteStride);
                hash_combine(seed, srvDesc.Buffer.Flags);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE1D:
                hash_combine(seed, srvDesc.Texture1D.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture1D.MipLevels);
                hash_combine(seed, srvDesc.Texture1D.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
                hash_combine(seed, srvDesc.Texture1DArray.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture1DArray.MipLevels);
                hash_combine(seed, srvDesc.Texture1DArray.FirstArraySlice);
                hash_combine(seed, srvDesc.Texture1DArray.ArraySize);
                hash_combine(seed, srvDesc.Texture1DArray.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2D:
                hash_combine(seed, srvDesc.Texture2D.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture2D.MipLevels);
                hash_combine(seed, srvDesc.Texture2D.PlaneSlice);
                hash_combine(seed, srvDesc.Texture2D.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                hash_combine(seed, srvDesc.Texture2DArray.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture2DArray.MipLevels);
                hash_combine(seed, srvDesc.Texture2DArray.FirstArraySlice);
                hash_combine(seed, srvDesc.Texture2DArray.ArraySize);
                hash_combine(seed, srvDesc.Texture2DArray.PlaneSlice);
                hash_combine(seed, srvDesc.Texture2DArray.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMS:
                //                hash_combine(seed, srvDesc.Texture2DMS.UnusedField_NothingToDefine);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
                hash_combine(seed, srvDesc.Texture2DMSArray.FirstArraySlice);
                hash_combine(seed, srvDesc.Texture2DMSArray.ArraySize);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE3D:
                hash_combine(seed, srvDesc.Texture3D.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture3D.MipLevels);
                hash_combine(seed, srvDesc.Texture3D.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURECUBE:
                hash_combine(seed, srvDesc.TextureCube.MostDetailedMip);
                hash_combine(seed, srvDesc.TextureCube.MipLevels);
                hash_combine(seed, srvDesc.TextureCube.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
                hash_combine(seed, srvDesc.TextureCubeArray.MostDetailedMip);
                hash_combine(seed, srvDesc.TextureCubeArray.MipLevels);
                hash_combine(seed, srvDesc.TextureCubeArray.First2DArrayFace);
                hash_combine(seed, srvDesc.TextureCubeArray.NumCubes);
                hash_combine(seed, srvDesc.TextureCubeArray.ResourceMinLODClamp);
                break;
                // TODO: Update Visual Studio?
                //case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
                //    hash_combine(seed, srvDesc.RaytracingAccelerationStructure.Location);
                //    break;
            }

            return seed;
        }
    };

    template<>
    struct hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>
    {
        std::size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc) const noexcept
        {
            std::size_t seed = 0;

            hash_combine(seed, uavDesc.Format);
            hash_combine(seed, uavDesc.ViewDimension);

            switch (uavDesc.ViewDimension)
            {
            case D3D12_UAV_DIMENSION_BUFFER:
                hash_combine(seed, uavDesc.Buffer.FirstElement);
                hash_combine(seed, uavDesc.Buffer.NumElements);
                hash_combine(seed, uavDesc.Buffer.StructureByteStride);
                hash_combine(seed, uavDesc.Buffer.CounterOffsetInBytes);
                hash_combine(seed, uavDesc.Buffer.Flags);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE1D:
                hash_combine(seed, uavDesc.Texture1D.MipSlice);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
                hash_combine(seed, uavDesc.Texture1DArray.MipSlice);
                hash_combine(seed, uavDesc.Texture1DArray.FirstArraySlice);
                hash_combine(seed, uavDesc.Texture1DArray.ArraySize);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE2D:
                hash_combine(seed, uavDesc.Texture2D.MipSlice);
                hash_combine(seed, uavDesc.Texture2D.PlaneSlice);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
                hash_combine(seed, uavDesc.Texture2DArray.MipSlice);
                hash_combine(seed, uavDesc.Texture2DArray.FirstArraySlice);
                hash_combine(seed, uavDesc.Texture2DArray.ArraySize);
                hash_combine(seed, uavDesc.Texture2DArray.PlaneSlice);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE3D:
                hash_combine(seed, uavDesc.Texture3D.MipSlice);
                hash_combine(seed, uavDesc.Texture3D.FirstWSlice);
                hash_combine(seed, uavDesc.Texture3D.WSize);
                break;
            }

            return seed;
        }
    };
}