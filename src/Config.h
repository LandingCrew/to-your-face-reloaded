#pragma once

/**
 * Filter mode determines how angle and distance filters are combined
 */
enum class FilterMode
{
	AngleOnly = 0,     // Original behavior - angle-based filtering only
	DistanceOnly = 1,  // Distance-based filtering only
	Both = 2,          // Both angle AND distance required (strict)
	Either = 3         // Either angle OR distance (permissive)
};

/**
 * Plugin configuration structure holding all settings.
 * Loaded from to-your-face-reloaded.ini at plugin initialization.
 */
struct PluginConfig
{
	// Angle-based filtering (existing feature)
	float maxDeviationAngle;  // Maximum angle in radians for allowing comments

	// Distance-based filtering (new feature)
	float maxGreetingDistance;        // Maximum distance in game units for comments
	float maxGreetingDistanceSquared; // Squared distance (optimization to avoid sqrt)

	// Close range bypass (new feature)
	bool enableCloseRangeBypass;      // Allow comments at close range regardless of angle
	float closeRangeDistance;         // Distance threshold for close range bypass
	float closeRangeDistanceSquared;  // Squared close range distance (optimization)

	// Filter mode (new feature)
	FilterMode filterMode;  // How to combine angle and distance filters

	// Debug logging (for troubleshooting)
	bool enableDebugLogging;  // Log each NPC comment check to help diagnose issues
};

// Global configuration instance
inline PluginConfig g_config;

// Constants
inline constexpr std::string_view kConfigFile = "Data\\SKSE\\Plugins\\to-your-face-reloaded.ini"sv;
inline constexpr float pi = 3.1415f;  // Probably overkill for this mod

/**
 * Loads plugin configuration from to-your-face-reloaded.ini file.
 * Provides backward compatibility - if new settings are missing,
 * defaults to angle-only filtering.
 *
 * @return true if configuration was loaded successfully, false otherwise
 */
bool LoadConfiguration();
