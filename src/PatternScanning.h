#pragma once

#include "PCH.h"

/**
 * CPU feature flags for SIMD optimization
 */
struct CPUFeatures {
	bool sse2;
	bool avx2;
};

/**
 * Detects CPU SIMD capabilities using CPUID instruction.
 * Also verifies OS support for AVX/AVX2 (requires OS to save/restore YMM registers).
 * @return CPUFeatures struct with sse2 and avx2 flags
 */
CPUFeatures DetectCPUFeatures();

/**
 * Scalar (byte-by-byte) pattern scanner - fallback implementation.
 * @param start Starting address to scan from
 * @param end Ending address (exclusive)
 * @param pattern Pattern bytes to search for
 * @param pattern_len Length of pattern in bytes
 * @return Address where pattern was found, or 0 if not found
 */
uintptr_t ScanPattern_Scalar(uintptr_t start, uintptr_t end,
                             const uint8_t* pattern, size_t pattern_len);

/**
 * SSE2-optimized pattern scanner using 128-bit SIMD.
 * @param start Starting address to scan from
 * @param end Ending address (exclusive)
 * @param pattern Pattern bytes to search for
 * @param pattern_len Length of pattern in bytes
 * @return Address where pattern was found, or 0 if not found
 */
uintptr_t ScanPattern_SSE2(uintptr_t start, uintptr_t end,
                          const uint8_t* pattern, size_t pattern_len);

/**
 * AVX2-optimized pattern scanner using 256-bit SIMD.
 * @param start Starting address to scan from
 * @param end Ending address (exclusive)
 * @param pattern Pattern bytes to search for
 * @param pattern_len Length of pattern in bytes
 * @return Address where pattern was found, or 0 if not found
 */
uintptr_t ScanPattern_AVX2(uintptr_t start, uintptr_t end,
                          const uint8_t* pattern, size_t pattern_len);

/**
 * Scans Skyrim's binary to locate the NPC comment function.
 * Uses pattern matching with SIMD optimizations (AVX2/SSE2/Scalar).
 * @return Address of the comment function, or std::nullopt if not found
 */
std::optional<uintptr_t> GetCommentAddress();

/**
 * Pattern bytes for NPC comment function
 * F3 0F 59 F6 0F B6 EB B8 01 00 00 00 0F 2F F0 0F 43 E8
 *
 * This pattern represents:
 *   mulss xmm6,xmm6      - Square the distance
 *   movzx ebp,bl         - Zero-extend result flag
 *   mov eax,1            - Load constant 1
 *   comiss xmm6,xmm0     - Compare squared distance
 *   cmovae ebp,eax       - Conditional move based on comparison
 */
inline constexpr uint8_t kCommentBytes[] = {
	0xF3, 0x0F, 0x59, 0xF6,         // mulss xmm6,xmm6
	0x0F, 0xB6, 0xEB,               // movzx ebp,bl
	0xB8, 0x01, 0x00, 0x00, 0x00,   // mov eax,1
	0x0F, 0x2F, 0xF0,               // comiss xmm6,xmm0
	0x0F, 0x43, 0xE8                // cmovae ebp,eax
};
inline constexpr size_t kCommentByteCount = sizeof(kCommentBytes);
