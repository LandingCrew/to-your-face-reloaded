#pragma once

#include "PCH.h"
#include "Config.h"  // For pi constant

/**
 * Determines whether an NPC should be allowed to make a comment to the player.
 * Applies configured filters based on angle, distance, and filter mode.
 *
 * Filter Modes:
 *   - ANGLE_ONLY: Only check if player is facing NPC
 *   - DISTANCE_ONLY: Only check if NPC is within distance threshold
 *   - BOTH: Require BOTH angle AND distance checks to pass
 *   - EITHER: Allow comment if EITHER angle OR distance check passes
 *
 * Special Features:
 *   - Close Range Bypass: If enabled, allows comments at close range regardless of angle
 *   - 3D Distance: Includes Z-axis in distance calculations for vertical awareness
 *   - Optimized: Uses squared distances to avoid expensive sqrt() calls
 *
 * @param npc Pointer to the NPC character attempting to comment
 * @return true if comment is allowed, false otherwise
 */
bool AllowComment(RE::Character* npc);
