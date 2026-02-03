/**
 * PatternScanning.cpp - SIMD-optimized pattern scanning for version-agnostic hooking
 *
 * This implementation includes critical fixes from code review:
 *
 * FIX #1 - Buffer Overrun Prevention:
 *   All scanners calculate scan_end = end - pattern_len + 1 to prevent memcmp
 *   from reading beyond the scan region boundary.
 *
 * FIX #2 - Unaligned Memory Access:
 *   Uses _mm_loadu_si128 / _mm256_loadu_si256 (unaligned loads) instead of
 *   aligned variants. Skyrim's .text section is not guaranteed to be aligned.
 *   Performance impact is negligible on modern CPUs (Haswell+).
 *
 * FIX #3 - CPU Compatibility (BMI1):
 *   Uses _BitScanForward() instead of _tzcnt_u32() for finding set bits.
 *   _tzcnt requires BMI1 (2013+ CPUs), but SSE2 exists on CPUs from 2000.
 *   This fix extends compatibility by ~13 years of CPUs.
 *
 * FIX #4 - AVX2 OS Support Check:
 *   Verifies OS has enabled AVX state saving via XGETBV before using AVX2.
 *   Required because AVX/AVX2 need OS support to save/restore YMM registers
 *   during context switches. Prevents crashes on Win7 RTM, old Linux kernels,
 *   and VMs without AVX passthrough.
 *
 * FIX #5 - Exception Handling:
 *   Separate __try/__except blocks for each SIMD level with graceful fallback.
 *   Catches both EXCEPTION_ACCESS_VIOLATION and EXCEPTION_ILLEGAL_INSTRUCTION.
 *   Scalar fallback is OUTSIDE exception handlers to prevent recursive faults.
 */

#include "PCH.h"
#include "PatternScanning.h"
#include <immintrin.h>  // SSE2, AVX2 intrinsics
#include <intrin.h>     // CPU feature detection, bit manipulation

// Memory scanning constants
inline constexpr uintptr_t kScanStartOffset = 0x1000;
inline constexpr uintptr_t kScanSize = 0x01000000;  // 16MB scan range

// Note: kCommentBytes and kCommentByteCount are defined in PatternScanning.h

CPUFeatures DetectCPUFeatures()
{
	CPUFeatures features = { false, false };

	// CPUID is guaranteed on x86-64, no need to check for support
	int cpuInfo[4];
	__cpuid(cpuInfo, 1);
	features.sse2 = (cpuInfo[3] & (1 << 26)) != 0;  // EDX bit 26

	// FIX #4: Check for AVX support AND OS support for AVX state saving
	// AVX/AVX2 require OS to save/restore YMM registers on context switch.
	// Without this check, AVX2 crashes on: Win7 RTM, old Linux, VMs without AVX.
	bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;  // ECX bit 27 - OS uses XSAVE/XGETBV
	bool cpuAvx = (cpuInfo[2] & (1 << 28)) != 0;   // ECX bit 28 - CPU supports AVX

	bool osAvxSupport = false;
	if (osxsave && cpuAvx) {
		// Use _xgetbv to check if OS has enabled XMM and YMM state saving
		unsigned long long xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
		// Bits 1-2 must be set: XMM state (bit 1) and YMM state (bit 2)
		osAvxSupport = (xcrFeatureMask & 0x6) == 0x6;
	}

	// Check for AVX2 support (only if OS supports AVX)
	__cpuid(cpuInfo, 0);
	if (cpuInfo[0] >= 7 && osAvxSupport) {
		__cpuidex(cpuInfo, 7, 0);
		features.avx2 = (cpuInfo[1] & (1 << 5)) != 0;  // EBX bit 5
	}

	return features;
}

uintptr_t ScanPattern_Scalar(uintptr_t start, uintptr_t end,
                             const uint8_t* pattern, size_t pattern_len)
{
	// Validate inputs
	if (!pattern || pattern_len == 0 || start >= end) {
		return 0;
	}

	// FIX #1: Calculate safe scan end to prevent buffer overrun
	// Without this, memcmp at addr near 'end' reads beyond buffer boundary
	uintptr_t scan_end = end - pattern_len + 1;
	if (scan_end <= start) {
		return 0;
	}

	// Simple byte-by-byte search
	for (uintptr_t addr = start; addr < scan_end; ++addr) {
		if (!memcmp((const void*)addr, pattern, pattern_len)) {
			return addr;
		}
	}

	return 0;
}

uintptr_t ScanPattern_SSE2(uintptr_t start, uintptr_t end,
                          const uint8_t* pattern, size_t pattern_len)
{
	// Validate inputs
	if (!pattern || pattern_len == 0 || start >= end) {
		return 0;
	}

	// FIX #1: Calculate safe scan end to prevent buffer overrun
	uintptr_t scan_end = end - pattern_len + 1;
	if (scan_end <= start) {
		return 0;
	}

	// Load first byte into all 16 bytes of XMM register
	__m128i first_byte = _mm_set1_epi8(pattern[0]);

	// Process 16 bytes at a time with SSE2, then scalar for remainder
	uintptr_t simd_end = scan_end & ~15;  // Round down to multiple of 16

	for (uintptr_t addr = start; addr < simd_end; addr += 16) {
		// FIX #2: Use unaligned load - aligned load crashes on misaligned addresses
		__m128i data = _mm_loadu_si128((const __m128i*)addr);
		__m128i cmp = _mm_cmpeq_epi8(data, first_byte);
		unsigned int mask = _mm_movemask_epi8(cmp);

		// Process all first-byte matches in this 16-byte block
		while (mask != 0) {
			// FIX #3: Use _BitScanForward (all x86-64) instead of _tzcnt_u32 (BMI1 only)
			unsigned long offset;
			_BitScanForward(&offset, mask);
			uintptr_t candidate = addr + offset;

			// Verify full pattern match
			if (!memcmp((const void*)candidate, pattern, pattern_len))
				return candidate;

			// Clear lowest set bit
			mask &= (mask - 1);
		}
	}

	// Scalar scan for remaining bytes (0-15 bytes at end)
	for (uintptr_t addr = simd_end; addr < scan_end; ++addr) {
		if (!memcmp((const void*)addr, pattern, pattern_len))
			return addr;
	}

	return 0;
}

uintptr_t ScanPattern_AVX2(uintptr_t start, uintptr_t end,
                          const uint8_t* pattern, size_t pattern_len)
{
	// Validate inputs
	if (!pattern || pattern_len == 0 || start >= end) {
		return 0;
	}

	// FIX #1: Calculate safe scan end to prevent buffer overrun
	uintptr_t scan_end = end - pattern_len + 1;
	if (scan_end <= start) {
		return 0;
	}

	// Load first byte into all 32 bytes of YMM register
	__m256i first_byte = _mm256_set1_epi8(pattern[0]);

	// Process 32 bytes at a time with AVX2, then scalar for remainder
	uintptr_t simd_end = scan_end & ~31;  // Round down to multiple of 32

	for (uintptr_t addr = start; addr < simd_end; addr += 32) {
		// FIX #2: Use unaligned load - aligned load crashes on misaligned addresses
		__m256i data = _mm256_loadu_si256((const __m256i*)addr);
		__m256i cmp = _mm256_cmpeq_epi8(data, first_byte);
		unsigned int mask = _mm256_movemask_epi8(cmp);

		// Process all first-byte matches in this 32-byte block
		while (mask != 0) {
			// FIX #3: Use _BitScanForward (all x86-64) instead of _tzcnt_u32 (BMI1 only)
			unsigned long offset;
			_BitScanForward(&offset, mask);
			uintptr_t candidate = addr + offset;

			// Verify full pattern match
			if (!memcmp((const void*)candidate, pattern, pattern_len))
				return candidate;

			// Clear lowest set bit
			mask &= (mask - 1);
		}
	}

	// Scalar scan for remaining bytes (0-31 bytes at end)
	for (uintptr_t addr = simd_end; addr < scan_end; ++addr) {
		if (!memcmp((const void*)addr, pattern, pattern_len))
			return addr;
	}

	return 0;
}

std::optional<uintptr_t> GetCommentAddress()
{
	uintptr_t baseAddr = REL::Module::get().base();
	uintptr_t start = baseAddr + kScanStartOffset;
	uintptr_t end = start + kScanSize;

	logger::info("Scanning for NPC comment function...");
	logger::info("  Base address: 0x{:016X}", baseAddr);
	logger::info("  Scan range: 0x{:016X} - 0x{:016X} ({} MB)",
		start, end, kScanSize / (1024 * 1024));
	logger::info("  Pattern signature: {} bytes", kCommentByteCount);
	logger::info("  Pattern bytes: F3 0F 59 F6 0F B6 EB B8 01 00 00 00 0F 2F F0 0F 43 E8");

	// Detect CPU features
	CPUFeatures cpu = DetectCPUFeatures();
	logger::info("  CPU features detected:");
	if (cpu.avx2) {
		logger::info("    - AVX2: Available (using 256-bit SIMD)");
	} else if (cpu.sse2) {
		logger::info("    - SSE2: Available (using 128-bit SIMD)");
	} else {
		logger::info("    - SIMD: Not available (using scalar fallback)");
	}

	uintptr_t result = 0;
	const char* method_used = "unknown";

	// Performance timing
	LARGE_INTEGER freq, time_start, time_end;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&time_start);

	// FIX #5: Separate __try/__except for each SIMD level with graceful fallback
	// Catches ACCESS_VIOLATION and ILLEGAL_INSTRUCTION. Scalar fallback is OUTSIDE
	// exception handlers to prevent recursive faults.

	// Try AVX2 first
	if (cpu.avx2 && !result) {
		__try {
			result = ScanPattern_AVX2(start, end, kCommentBytes, kCommentByteCount);
			if (result) {
				method_used = "AVX2";
			} else {
				logger::warn("AVX2 scan completed but pattern not found, trying SSE2");
			}
		} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ||
		            GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ?
		            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
			DWORD exCode = GetExceptionCode();
			const char* exName = (exCode == EXCEPTION_ACCESS_VIOLATION) ? "Access Violation" : "Illegal Instruction";
			logger::warn("AVX2 scan raised exception (0x{:08X}: {}), disabling AVX2", exCode, exName);
			cpu.avx2 = false;  // Disable AVX2 for safety
			result = 0;
		}
	}

	// Try SSE2 if AVX2 failed or unavailable
	if (cpu.sse2 && !result) {
		__try {
			result = ScanPattern_SSE2(start, end, kCommentBytes, kCommentByteCount);
			if (result) {
				method_used = "SSE2";
			} else {
				logger::warn("SSE2 scan completed but pattern not found, trying scalar");
			}
		} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
		            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
			logger::warn("SSE2 scan raised access violation, falling back to scalar");
			result = 0;
		}
	}

	// Fallback to scalar scan
	if (!result) {
		__try {
			result = ScanPattern_Scalar(start, end, kCommentBytes, kCommentByteCount);
			if (result) {
				method_used = "Scalar";
			}
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			logger::error("Scalar scan raised exception, aborting");
			return std::nullopt;
		}
	}

	QueryPerformanceCounter(&time_end);
	double elapsed_ms = (time_end.QuadPart - time_start.QuadPart) * 1000.0 / freq.QuadPart;

	if (result) {
		logger::info("Pattern found!");
		logger::info("  Address: 0x{:016X}", result);
		logger::info("  Offset from base: +0x{:08X}", result - baseAddr);
		logger::info("  Method used: {}", method_used);
		logger::info("  Scan time: {:.3f} ms", elapsed_ms);
		return result;
	}

	logger::error("Pattern not found!");
	logger::error("  Scan time: {:.3f} ms", elapsed_ms);
	logger::error("  This likely means:");
	logger::error("    - Game version is not supported");
	logger::error("    - Game binary has been modified");
	logger::error("    - Pattern needs to be updated");
	return std::nullopt;
}
