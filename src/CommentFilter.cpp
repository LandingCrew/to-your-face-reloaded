#include "PCH.h"
#include "CommentFilter.h"
#include "Config.h"

namespace
{
	/**
	 * Checks if the player is facing toward an NPC within the allowed deviation angle.
	 *
	 * @param npc Pointer to the NPC character
	 * @param dx Delta X between NPC and player
	 * @param dy Delta Y between NPC and player
	 * @return true if player is facing the NPC, false otherwise
	 */
	inline bool IsPlayerFacingNPC(RE::Character* npc, float dx, float dy)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return true;
		}

		// Calculate angle from player to NPC
		float angle = atan2(dx, dy);  // x,y: clockwise; 0 at the top
		if (angle < 0.0f) {
			angle += pi * 2.0f;
		}

		// Calculate deviation from player's facing direction
		float playerAngle = player->GetAngleZ();  // Get player's yaw rotation in radians
		float deviation = fabs(angle - playerAngle);
		if (deviation > pi) {
			deviation = 2.0f * pi - deviation;
		}

		return deviation < g_config.maxDeviationAngle;
	}

	/**
	 * Checks if the NPC is within the maximum greeting distance.
	 *
	 * @param distanceSquared Squared distance between NPC and player
	 * @return true if NPC is within range, false otherwise
	 */
	inline bool IsWithinGreetingDistance(float distanceSquared)
	{
		return distanceSquared <= g_config.maxGreetingDistanceSquared;
	}

	/**
	 * Checks if the NPC is within close range (for bypass feature).
	 *
	 * @param distanceSquared Squared distance between NPC and player
	 * @return true if NPC is within close range, false otherwise
	 */
	inline bool IsWithinCloseRange(float distanceSquared)
	{
		return distanceSquared <= g_config.closeRangeDistanceSquared;
	}
}

bool AllowComment(RE::Character* npc)
{
	// Sanity checks - allow comment if we can't properly evaluate
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!npc || !player || npc == player) {
		if (g_config.enableDebugLogging) {
			logger::info("[AllowComment] Sanity check: npc={}, player={}, same={} -> ALLOW",
				npc ? "valid" : "null",
				player ? "valid" : "null",
				(npc == player) ? "yes" : "no");
		}
		return true;
	}

	// Get NPC name for logging (if enabled)
	const char* npcName = nullptr;
	if (g_config.enableDebugLogging) {
		auto baseForm = npc->GetActorBase();
		if (baseForm) {
			npcName = baseForm->GetName();
		}
		if (!npcName || npcName[0] == '\0') {
			npcName = "Unknown";
		}
	}

	// Calculate position deltas
	float dx = npc->GetPositionX() - player->GetPositionX();
	float dy = npc->GetPositionY() - player->GetPositionY();
	float dz = npc->GetPositionZ() - player->GetPositionZ();

	// Calculate 3D distance squared (includes Z-axis for vertical awareness)
	float distanceSquared = dx * dx + dy * dy + dz * dz;
	float distance = sqrt(distanceSquared);  // Only calculate sqrt if logging

	// Close Range Bypass: Allow all angles at very close range if enabled
	// This prevents NPCs from being silent when standing right next to the player
	if (g_config.enableCloseRangeBypass && IsWithinCloseRange(distanceSquared)) {
		if (g_config.enableDebugLogging) {
			logger::info("[AllowComment] \"{}\" dist={:.1f} -> ALLOW (close range bypass)",
				npcName, distance);
		}
		return true;  // Early exit - allow comment regardless of angle
	}

	bool result = false;
	const char* reason = "unknown";

	// Apply filters based on configured filter mode
	switch (g_config.filterMode) {
		case FilterMode::AngleOnly:
			// Only check angle
			result = IsPlayerFacingNPC(npc, dx, dy);
			reason = result ? "facing" : "not facing";
			break;

		case FilterMode::DistanceOnly:
			// Only check distance, ignore angle
			result = IsWithinGreetingDistance(distanceSquared);
			reason = result ? "in range" : "out of range";
			break;

		case FilterMode::Both:
			// Require BOTH angle AND distance checks to pass
			// Check distance first (cheap) before angle (expensive atan2)
			if (!IsWithinGreetingDistance(distanceSquared)) {
				result = false;
				reason = "out of range";
			} else if (!IsPlayerFacingNPC(npc, dx, dy)) {
				result = false;
				reason = "not facing";
			} else {
				result = true;
				reason = "facing AND in range";
			}
			break;

		case FilterMode::Either:
			// Allow if EITHER angle OR distance check passes
			// Check distance first (cheap) before angle (expensive atan2)
			if (IsWithinGreetingDistance(distanceSquared)) {
				result = true;
				reason = "in range";
			} else if (IsPlayerFacingNPC(npc, dx, dy)) {
				result = true;
				reason = "facing";
			} else {
				result = false;
				reason = "not facing AND out of range";
			}
			break;

		default:
			// Fallback to angle-only mode for safety
			result = IsPlayerFacingNPC(npc, dx, dy);
			reason = result ? "facing (fallback)" : "not facing (fallback)";
			break;
	}

	if (g_config.enableDebugLogging) {
		logger::info("[AllowComment] \"{}\" dist={:.1f} -> {} ({})",
			npcName, distance, result ? "ALLOW" : "BLOCK", reason);
	}

	return result;
}
