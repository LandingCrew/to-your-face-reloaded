#include "PCH.h"
#include "Config.h"

namespace
{
	/**
	 * Helper function to read a boolean value from INI file.
	 * Supports multiple formats: true/false, yes/no, 1/0
	 */
	bool GetPrivateProfileBool(const char* section, const char* key, bool defaultValue, const char* filename)
	{
		char buffer[256];
		GetPrivateProfileStringA(section, key, defaultValue ? "true" : "false", buffer, sizeof(buffer), filename);

		// Convert to lowercase for case-insensitive comparison
		std::string value = buffer;
		for (char& c : value) {
			c = static_cast<char>(tolower(c));
		}

		// Check various boolean representations
		if (value == "true" || value == "yes" || value == "1" || value == "on" || value == "enabled") {
			return true;
		}
		if (value == "false" || value == "no" || value == "0" || value == "off" || value == "disabled") {
			return false;
		}

		// Return default if value is unrecognized
		return defaultValue;
	}

	/**
	 * Helper function to read a float value from INI file.
	 */
	float GetPrivateProfileFloat(const char* section, const char* key, float defaultValue, const char* filename)
	{
		char buffer[256];
		char defaultStr[64];
		sprintf_s(defaultStr, sizeof(defaultStr), "%.2f", defaultValue);

		GetPrivateProfileStringA(section, key, defaultStr, buffer, sizeof(buffer), filename);

		return static_cast<float>(atof(buffer));
	}

	/**
	 * Parses filter mode from string value.
	 * Supports: "Angle", "Distance", "Both", "Either" (case-insensitive)
	 */
	FilterMode ParseFilterMode(const char* modeStr)
	{
		std::string mode = modeStr;

		// Convert to lowercase for case-insensitive comparison
		for (char& c : mode) {
			c = static_cast<char>(tolower(c));
		}

		if (mode == "angle" || mode == "angleonly" || mode == "angle_only") {
			return FilterMode::AngleOnly;
		} else if (mode == "distance" || mode == "distanceonly" || mode == "distance_only") {
			return FilterMode::DistanceOnly;
		} else if (mode == "both" || mode == "and") {
			return FilterMode::Both;
		} else if (mode == "either" || mode == "or") {
			return FilterMode::Either;
		}

		// Default to angle-only for backward compatibility
		return FilterMode::AngleOnly;
	}
}

bool LoadConfiguration()
{
	logger::info("Loading configuration from: {}", kConfigFile);

	// Check if config file exists
	FILE* testFile = nullptr;
	errno_t err = fopen_s(&testFile, kConfigFile.data(), "r");
	if (err != 0 || !testFile) {
		logger::warn("Configuration file not found - using defaults");
	} else {
		logger::info("Configuration file found and readable");
		fclose(testFile);
	}

	// ========================================
	// [Main] Section
	// ========================================

	logger::info("Loading [Main] section...");

	// Load MaxDeviationAngle (existing setting)
	int deviationAngleDegrees = GetPrivateProfileIntA("Main", "MaxDeviationAngle", 30, kConfigFile.data());
	const int originalAngle = deviationAngleDegrees;

	// Validate and clamp angle to valid range [0, 180]
	if (deviationAngleDegrees < 0) {
		logger::warn("  MaxDeviationAngle ({}) is negative, clamping to 0", deviationAngleDegrees);
		deviationAngleDegrees = 0;
	}
	if (deviationAngleDegrees > 180) {
		logger::warn("  MaxDeviationAngle ({}) exceeds 180, clamping to 180", deviationAngleDegrees);
		deviationAngleDegrees = 180;
	}

	g_config.maxDeviationAngle = deviationAngleDegrees / 180.0f * pi;

	if (originalAngle == deviationAngleDegrees) {
		logger::info("  MaxDeviationAngle: {} degrees ({:.4f} radians) - Value OK", deviationAngleDegrees, g_config.maxDeviationAngle);
	} else {
		logger::info("  MaxDeviationAngle: {} degrees ({:.4f} radians) - Clamped from {}", deviationAngleDegrees, g_config.maxDeviationAngle, originalAngle);
	}

	// Load FilterMode (new setting)
	char filterModeStr[256];
	GetPrivateProfileStringA("Main", "FilterMode", "Angle", filterModeStr, sizeof(filterModeStr), kConfigFile.data());

	logger::info("  FilterMode (raw): \"{}\"", filterModeStr);

	g_config.filterMode = ParseFilterMode(filterModeStr);

	constexpr std::array filterModeNames = { "Angle Only", "Distance Only", "Both (AND)", "Either (OR)" };
	logger::info("  FilterMode (parsed): {}", filterModeNames[static_cast<int>(g_config.filterMode)]);

	// ========================================
	// [Distance] Section
	// ========================================

	logger::info("Loading [Distance] section...");

	// Load MaxGreetingDistance
	const float rawMaxDistance = GetPrivateProfileFloat("Distance", "MaxGreetingDistance", 150.0f, kConfigFile.data());
	g_config.maxGreetingDistance = rawMaxDistance;

	logger::info("  MaxGreetingDistance (raw): {:.2f} units", rawMaxDistance);

	// Validate distance is positive
	if (g_config.maxGreetingDistance < 0.0f) {
		logger::warn("  MaxGreetingDistance ({:.2f}) is negative, using absolute value", g_config.maxGreetingDistance);
		g_config.maxGreetingDistance = std::abs(g_config.maxGreetingDistance);
		logger::info("  MaxGreetingDistance (corrected): {:.2f} units", g_config.maxGreetingDistance);
	}

	// Pre-calculate squared distance for performance
	g_config.maxGreetingDistanceSquared = g_config.maxGreetingDistance * g_config.maxGreetingDistance;
	logger::info("  MaxGreetingDistance: {:.2f} units ({:.2f} squared)", g_config.maxGreetingDistance, g_config.maxGreetingDistanceSquared);

	// Load CloseRangeBypass
	g_config.enableCloseRangeBypass = GetPrivateProfileBool("Distance", "CloseRangeBypass", false, kConfigFile.data());
	logger::info("  CloseRangeBypass: {} {}", g_config.enableCloseRangeBypass ? "ENABLED" : "DISABLED", g_config.enableCloseRangeBypass ? "" : "(default)");

	// Load CloseRangeDistance
	const float rawCloseDistance = GetPrivateProfileFloat("Distance", "CloseRangeDistance", 50.0f, kConfigFile.data());
	g_config.closeRangeDistance = rawCloseDistance;

	if (g_config.enableCloseRangeBypass) {
		logger::info("  CloseRangeDistance (raw): {:.2f} units", rawCloseDistance);

		// Validate distance is positive
		if (g_config.closeRangeDistance < 0.0f) {
			logger::warn("  CloseRangeDistance ({:.2f}) is negative, using absolute value", g_config.closeRangeDistance);
			g_config.closeRangeDistance = std::abs(g_config.closeRangeDistance);
			logger::info("  CloseRangeDistance (corrected): {:.2f} units", g_config.closeRangeDistance);
		}

		// Pre-calculate squared distance for performance
		g_config.closeRangeDistanceSquared = g_config.closeRangeDistance * g_config.closeRangeDistance;
		logger::info("  CloseRangeDistance: {:.2f} units ({:.2f} squared)", g_config.closeRangeDistance, g_config.closeRangeDistanceSquared);
	} else {
		// Still calculate for consistency, but don't log details
		g_config.closeRangeDistanceSquared = g_config.closeRangeDistance * g_config.closeRangeDistance;
	}

	// Validate CloseRangeDistance <= MaxGreetingDistance
	if (g_config.enableCloseRangeBypass && g_config.closeRangeDistance > g_config.maxGreetingDistance) {
		logger::warn("  CloseRangeDistance ({:.2f}) is greater than MaxGreetingDistance ({:.2f})",
			g_config.closeRangeDistance, g_config.maxGreetingDistance);
		logger::warn("  This creates confusing behavior - clamping CloseRangeDistance to MaxGreetingDistance");
		g_config.closeRangeDistance = g_config.maxGreetingDistance;
		g_config.closeRangeDistanceSquared = g_config.closeRangeDistance * g_config.closeRangeDistance;
		logger::info("  CloseRangeDistance (clamped): {:.2f} units ({:.2f} squared)",
			g_config.closeRangeDistance, g_config.closeRangeDistanceSquared);
	}

	// ========================================
	// [Debug] Section
	// ========================================

	logger::info("Loading [Debug] section...");

	g_config.enableDebugLogging = GetPrivateProfileBool("Debug", "EnableLogging", false, kConfigFile.data());
	if (g_config.enableDebugLogging) {
		logger::info("  EnableLogging: ENABLED - Will log each NPC comment check");
		logger::warn("  WARNING: Debug logging is verbose and may impact performance!");
	} else {
		logger::info("  EnableLogging: DISABLED (default)");
	}

	// ========================================
	// Configuration Summary
	// ========================================

	logger::info("--------------------------------------------------------");
	logger::info("Configuration Summary:");
	logger::info("--------------------------------------------------------");

	// Determine effective behavior mode
	if (g_config.filterMode == FilterMode::AngleOnly && !g_config.enableCloseRangeBypass) {
		logger::info("  Active Mode: ANGLE ONLY");
		logger::info("    NPCs will only comment when player faces them");
		logger::info("    Maximum deviation: {} degrees", deviationAngleDegrees);
	} else if (g_config.filterMode == FilterMode::DistanceOnly) {
		logger::info("  Active Mode: DISTANCE ONLY");
		logger::info("    NPCs will only comment when within {:.2f} units", g_config.maxGreetingDistance);
	} else if (g_config.filterMode == FilterMode::Both) {
		logger::info("  Active Mode: BOTH (angle AND distance required)");
		logger::info("    NPCs will only comment when within {:.2f} units AND within {} degrees", g_config.maxGreetingDistance, deviationAngleDegrees);
		if (g_config.enableCloseRangeBypass) {
			logger::info("    Exception: All angles allowed when < {:.2f} units", g_config.closeRangeDistance);
		}
	} else if (g_config.filterMode == FilterMode::Either) {
		logger::info("  Active Mode: EITHER (angle OR distance)");
		logger::info("    NPCs will comment when within {:.2f} units OR within {} degrees", g_config.maxGreetingDistance, deviationAngleDegrees);
	}

	logger::info("Configuration loaded successfully");

	return true;
}
