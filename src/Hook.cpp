/**
 * Hook.cpp - Runtime assembly injection using Xbyak
 *
 * This implementation includes fixes from code reviews:
 *
 * FIX #6 - Instruction Cache Flushing:
 *   Calls FlushInstructionCache() after modifying executable code.
 *   CPU instruction cache may contain stale instructions after code modification.
 *   While x86 has strong cache coherency, this is the correct and safe approach.
 *
 * FIX #7 - RAX Register Corruption:
 *   Changed jump trampoline from "mov rax; jmp rax" to "mov r11; jmp r11".
 *   The original approach corrupted RAX before the hook code could save it,
 *   causing crashes when later Skyrim code depended on RAX's original value.
 *   R11 is a volatile scratch register in Windows x64 ABI, safe to clobber.
 *   Crash signature: RAX containing hook buffer address (e.g., 0xFFFFFFFFCBE60002)
 *   instead of expected game data, leading to access violations.
 */

#include "PCH.h"
#include "Hook.h"
#include "CommentFilter.h"
#include "PatternScanning.h"  // For kCommentBytes, kCommentByteCount

inline constexpr size_t kMinJumpSize = 0xD;         // 13 bytes required for long jump (mov r11 + jmp r11)
inline constexpr size_t kHookBufferSize = 0x100;    // 256 bytes for generated hook code

namespace
{
	/**
	 * Writes a 64-bit long jump instruction at the specified address.
	 * Uses the pattern: mov r11, destination; jmp r11
	 *
	 * IMPORTANT: Uses R11 instead of RAX because R11 is a volatile scratch register
	 * in the Windows x64 ABI. Using RAX would corrupt it before the hook code can
	 * save it, causing crashes when later code depends on RAX's original value.
	 *
	 * @param source      Address where the jump will be written
	 * @param destination Address to jump to
	 * @param length      Number of bytes to overwrite (minimum 13)
	 */
	void WriteLongJmp64(void* source, void* destination, size_t length)
	{
		if (length < kMinJumpSize) {
			logger::error("WriteLongJmp64: length ({}) < minimum ({})", length, kMinJumpSize);
			return;
		}

		DWORD oldProtect;
		// mov r11, 0xABABABABABABABAB (10 bytes)
		// jmp r11 (3 bytes)
		// Total: 13 bytes
		uint8_t payload[] = {
			0x49, 0xBB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,  // mov r11, 0xABABABABABABABAB
			0x41, 0xFF, 0xE3                                              // jmp r11
		};

		if (!VirtualProtect(source, length, PAGE_EXECUTE_READWRITE, &oldProtect)) {
			DWORD errorCode = GetLastError();
			logger::error("VirtualProtect failed to make memory writable!");
			logger::error("  Address: 0x{:016X}, Size: {}", (uintptr_t)source, length);
			logger::error("  Error code: {} (0x{:08X})", errorCode, errorCode);
			return;
		}

		*(void**)(payload + 2) = destination;
		memcpy(source, payload, 13);
		memset((uint8_t*)source + 13, 0x90, length - 13);  // NOP padding

		if (!VirtualProtect(source, length, oldProtect, &oldProtect)) {
			// Non-fatal: memory is still valid, just with wrong protection
			logger::warn("VirtualProtect failed to restore original protection");
		}

		// FIX #6: Flush instruction cache to ensure CPU sees modified code
		FlushInstructionCache(GetCurrentProcess(), source, length);
	}
}

bool IsBinaryCompatible(uintptr_t commentAddress)
{
	logger::info("Verifying binary compatibility...");

	if (!commentAddress) {
		logger::error("Binary compatibility check FAILED - pattern address is NULL");
		return false;
	}

	bool compatible = !memcmp((void*)commentAddress, kCommentBytes, kCommentByteCount);

	if (compatible) {
		logger::info("Binary compatibility check: PASSED");
	} else {
		logger::error("Binary compatibility check: FAILED");
		logger::error("  Address: 0x{:016X}", commentAddress);
		logger::error("  Expected vs Found bytes:");

		// Show hex dump comparison
		uint8_t* foundBytes = (uint8_t*)commentAddress;
		for (size_t i = 0; i < kCommentByteCount; i++) {
			if (kCommentBytes[i] != foundBytes[i]) {
				logger::error("  Offset +{:02d}: Expected 0x{:02X}, Found 0x{:02X} <-- MISMATCH",
					i, kCommentBytes[i], foundBytes[i]);
			} else {
				logger::error("  Offset +{:02d}: Expected 0x{:02X}, Found 0x{:02X}",
					i, kCommentBytes[i], foundBytes[i]);
			}
		}
	}

	return compatible;
}

bool InstallCommentHook(uintptr_t commentAddress)
{
	logger::info("--------------------------------------------------------");
	logger::info("Installing comment hook...");
	logger::info("--------------------------------------------------------");

	// Xbyak code generator for the hook
	struct CommentHookCode : Xbyak::CodeGenerator
	{
		CommentHookCode(void* buf, uintptr_t returnAddr) : Xbyak::CodeGenerator(kHookBufferSize, buf)
		{
			// Initialize result to 0 (will be set by AllowComment)
			xor_(ebp, ebp);

			// Note: We intentionally skip the vanilla distance check (comiss/jae)
			// and let AllowComment handle all filtering logic including:
			// - Angle filtering
			// - Distance filtering
			// - Close range bypass
			// - Filter mode combinations

			// Call our AllowComment function
			push(rax);  // rax pushed twice to keep 16-byte alignment (x64 calling convention)
			push(rax);
			push(rcx);
			push(rdx);

			mov(rcx, rdi);  // RDI contains Character* npc
			mov(rax, (uintptr_t)AllowComment);
			call(rax);

			test(al, al);

			pop(rdx);
			pop(rcx);
			pop(rax);
			pop(rax);

			setnz(bpl);  // Set ebp based on AllowComment result

			// Restore eax to 1 to match vanilla behavior (original code did "mov eax, 1")
			// Some code after the hook may depend on this value
			mov(eax, 1);

			// Return to original code
			push(rax);
			mov(rax, returnAddr);
			xchg(rax, ptr[rsp]);
			ret();
		}
	};

	// Allocate executable memory for the hook
	void* hookBuffer = VirtualAlloc(nullptr, kHookBufferSize,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (!hookBuffer) {
		DWORD errorCode = GetLastError();
		logger::error("Failed to allocate hook buffer!");
		logger::error("  Error code: {} (0x{:08X})", errorCode, errorCode);
		logger::error("  Requested size: {} bytes", kHookBufferSize);
		logger::error("  This likely indicates system memory exhaustion");
		return false;
	}

	logger::info("Hook buffer allocated at: 0x{:016X}", (uintptr_t)hookBuffer);
	logger::info("Buffer size: {} bytes", kHookBufferSize);
	logger::info("Memory protection: PAGE_EXECUTE_READWRITE");

	logger::info("Generating hook code with Xbyak...");

	uintptr_t returnAddr = commentAddress + kCommentByteCount;
	CommentHookCode code(hookBuffer, returnAddr);

	size_t codeSize = code.getSize();
	logger::info("Hook code generated: {} bytes", codeSize);

	if (codeSize > kHookBufferSize) {
		logger::error("Generated code ({} bytes) exceeds buffer size ({} bytes)!",
			codeSize, kHookBufferSize);
		logger::error("  This is a critical internal error - aborting hook installation");
		VirtualFree(hookBuffer, 0, MEM_RELEASE);
		return false;
	}

	logger::info("Hook code size validation: OK ({}/{} bytes used)", codeSize, kHookBufferSize);

	logger::info("Installing jump at target address...");
	logger::info("  Jump source: 0x{:016X}", commentAddress);
	logger::info("  Jump target: 0x{:016X}", (uintptr_t)code.getCode());
	logger::info("  Overwrite size: {} bytes", kCommentByteCount);

	WriteLongJmp64((void*)commentAddress, (void*)code.getCode(), kCommentByteCount);

	logger::info("Long jump (mov r11, target; jmp r11) installed successfully");
	logger::info("Hook installation: SUCCESSFUL");
	logger::info("  AllowComment filter will now be called for all NPC comments");

	return true;
}
