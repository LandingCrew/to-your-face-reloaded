#pragma once

#include "PCH.h"

/**
 * Installs the runtime hook into Skyrim's comment function.
 * Generates x64 assembly code using Xbyak that:
 * 1. Executes the original distance check
 * 2. Calls AllowComment() to verify player is facing the NPC
 * 3. Returns to normal execution flow
 *
 * The hook preserves all registers and maintains proper x64 calling convention
 * with 16-byte stack alignment.
 *
 * @param commentAddress Address of the comment function to hook
 * @return true if hook installation succeeded, false otherwise
 */
bool InstallCommentHook(uintptr_t commentAddress);

/**
 * Verifies that the bytes at the target address match our expected pattern.
 * This ensures we're hooking the correct function and the game binary hasn't changed.
 *
 * @param commentAddress Address to verify
 * @return true if binary is compatible, false otherwise
 */
bool IsBinaryCompatible(uintptr_t commentAddress);
