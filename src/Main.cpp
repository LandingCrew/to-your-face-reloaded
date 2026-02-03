#include "PCH.h"
#include "Config.h"
#include "PatternScanning.h"
#include "Hook.h"
#include "CommentFilter.h"

namespace
{
	/**
	 * Setup logging to the SKSE log directory
	 */
	void SetupLog()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find SKSE log directory");
		}

		*path /= "to_your_face.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

		auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}
}

/**
 * SKSE plugin version information (modern CommonLibSSE-NG format)
 */
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("Fudgyduff (Enhanced by community)");
	v.UsesAddressLibrary(false);  // We use pattern scanning, not Address Library
	v.HasNoStructUse(true);
	// Pattern scanning is version-agnostic - manually declare all known AE versions
	v.CompatibleVersions({
		SKSE::RUNTIME_SSE_1_5_97,   // SE latest
		SKSE::RUNTIME_SSE_1_6_640,  // AE
		SKSE::RUNTIME_SSE_1_6_659,  // AE
		SKSE::RUNTIME_SSE_1_6_678,  // AE
		REL::Version(1, 6, 1170, 0) // AE 1.6.1170 (your version)
	});
	return v;
}();

/**
 * SKSE plugin query function
 */
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	// Editor check
	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible");
		return false;
	}

	// Version check
	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_SSE_1_5_39) {
		logger::critical("Unsupported runtime version: {}", ver.string());
		logger::critical("Minimum required: 1.5.39");
		return false;
	}

	logger::info("Runtime version: {} - Compatible", ver.string());

	// Pattern scan and binary compatibility check
	logger::info("");
	auto commentAddress = GetCommentAddress();
	if (!commentAddress) {
		logger::critical("Failed to locate NPC comment function!");
		logger::critical("  This plugin cannot function without hooking the comment system");
		logger::critical("  Possible causes:");
		logger::critical("    - Unsupported game version");
		logger::critical("    - Modified game executable");
		logger::critical("    - Pattern needs updating");
		return false;
	}

	if (!IsBinaryCompatible(*commentAddress)) {
		logger::critical("Binary compatibility check failed!");
		logger::critical("  The game executable has unexpected bytes at the hook location");
		logger::critical("  Installing the hook would likely cause crashes");
		logger::critical("  Please check for game updates or conflicting mods");
		return false;
	}

	return true;
}

/**
 * SKSE plugin load function
 */
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	// Initialize SKSE
	SKSE::Init(a_skse);

	// Set up logging
	SetupLog();

	logger::info("================================================================================");
	logger::info("{} v{}", Plugin::NAME, Plugin::VERSION.string());
	logger::info("Build: {} (commit: {})", Plugin::BUILD_TIME, Plugin::GIT_COMMIT);
	logger::info("Author: Fudgyduff (Enhanced by community)");
	logger::info("================================================================================");

	// Get runtime version
	const auto runtimeVersion = REL::Module::get().version();
	logger::info("  Runtime version: {}.{}.{}.{}", runtimeVersion[0], runtimeVersion[1], runtimeVersion[2], runtimeVersion[3]);

	// Load configuration
	logger::info("");
	if (!LoadConfiguration()) {
		logger::error("Failed to load configuration!");
		return false;
	}

	// Install hook
	logger::info("");
	auto commentAddress = GetCommentAddress();
	if (!commentAddress) {
		logger::error("Failed to locate NPC comment function - hook not installed!");
		logger::error("Plugin will load but will not function");
		return true;  // Don't fail completely, just warn
	}

	if (!InstallCommentHook(*commentAddress)) {
		logger::error("Failed to install comment hook!");
		logger::error("Plugin will load but will not function");
		return true;  // Don't fail completely, just warn
	}

	logger::info("");
	logger::info("================================================================================");
	logger::info("{} v{} - Initialization Complete", Plugin::NAME, Plugin::VERSION.string());
	logger::info("================================================================================");

	// Print final status summary
	constexpr std::array filterModeNames = { "ANGLE ONLY", "DISTANCE ONLY", "BOTH (AND)", "EITHER (OR)" };
	logger::info("[INFO] Final Status:");
	logger::info("  Plugin status: ACTIVE");
	logger::info("  Filter mode: {}", filterModeNames[static_cast<int>(g_config.filterMode)]);

	if (g_config.filterMode == FilterMode::AngleOnly || g_config.filterMode == FilterMode::Both || g_config.filterMode == FilterMode::Either) {
		logger::info("  Angle filtering: ENABLED (max deviation: {:.0f} degrees)", g_config.maxDeviationAngle * 180.0f / pi);
	}

	if (g_config.filterMode == FilterMode::DistanceOnly || g_config.filterMode == FilterMode::Both || g_config.filterMode == FilterMode::Either) {
		logger::info("  Distance filtering: ENABLED (max distance: {:.1f} units)", g_config.maxGreetingDistance);
	}

	if (g_config.enableCloseRangeBypass) {
		logger::info("  Close range bypass: ENABLED (threshold: {:.1f} units)", g_config.closeRangeDistance);
	}

	return true;
}
