#pragma once

#include "Log.h"


#ifdef _DEBUG
#define ENABLE_ASSERTS
#endif

#ifdef ENABLE_ASSERTS
#define ASSERT(x, ...) { if(!(x)) { LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define CORE_ASSERT(x, ...) { if(!(x)) { LOG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define CORE_FATAL_ASSERT(x, ...) { if(!(x)) { LOG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); std::exception(__VA_ARGS__);} }
#else
#define ASSERT(x, ...)
#define CORE_ASSERT(x, ...)
#define CORE_FATAL_ASSERT(x, ...)  { if(!(x)) { LOG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); std::exception(__VA_ARGS__);} }
#endif