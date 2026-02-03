#pragma once

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#pragma warning(disable: 4702)
#include <SimpleIni.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <xbyak/xbyak.h>
#pragma warning(pop)

// Standard library
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Platform headers
#include <ShlObj.h>
#include <intrin.h>     // CPU feature detection, bit manipulation
#include <immintrin.h>  // SSE2, AVX2 intrinsics

using namespace std::literals;

namespace logger = SKSE::log;

namespace util
{
	using SKSE::stl::report_and_fail;
}

#define DLLEXPORT __declspec(dllexport)

// Logging macros for compatibility
#define INFO(...)     logger::info(__VA_ARGS__)
#define ERROR(...)    logger::error(__VA_ARGS__)
#define WARN(...)     logger::warn(__VA_ARGS__)
#define DEBUG(...)    logger::debug(__VA_ARGS__)
#define TRACE(...)    logger::trace(__VA_ARGS__)
#define CRITICAL(...) logger::critical(__VA_ARGS__)

#include "Plugin.h"
