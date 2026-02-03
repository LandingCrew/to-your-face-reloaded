# To Your Face Reloaded (TYFR)

> **Disclaimer:** Claude Code (Opus 4.5) assisted with Documentation, Debugging, and Code Reviews. I don't know assembly (or extremely low-level C/C++ for that matter) and this update would not exist without the tool. I made this as a learning project - Assembly injection was actually really interesting to read about!
---

## About

**To Your Face Reloaded** is a modernized SKSE64 plugin for Skyrim SE/AE that ensures NPCs only make comments when the player is actually facing them. No more hearing "Need something?" while walking away!

This is a modernization of the original [To Your Face SE - AE - VR](https://www.nexusmods.com/skyrimspecialedition/mods/24720) by [sfts](https://www.nexusmods.com/skyrimspecialedition/users/261389), which was itself a re-release of To Your Face (ported by [xILARTH](https://www.nexusmods.com/profile/xILARTH?gameId=1704)), originally created by [underthesky](https://www.nexusmods.com/profile/underthesky?gameId=110).

---

## Features

### Core Functionality
- **Angle-Based Filtering**: NPCs only comment when you're facing them (configurable cone angle)
- **Distance-Based Filtering**: Optional distance threshold for more realistic greetings
- **Close Range Bypass**: Allow comments at very close range regardless of angle
- **Four Filter Modes**: AngleOnly, DistanceOnly, Both (AND), Either (OR)

### Technical Highlights
- **Version-Agnostic**: Pattern scanning adapts to any Skyrim SE/AE version automatically
- **Runtime Code Injection**: Uses Xbyak JIT compiler for safe, conflict-free hooking
- **No Address Library**: Works independently through byte pattern matching

### Improvements
#### Performance
- **SIMD-optimized pattern scanning** (AVX2/SSE2)
- Three-tier fallback: AVX2 → SSE2 → Scalar

#### New Features
- **Distance-Based Filtering**: Control greetings by proximity, not just angle
- **Close Range Bypass**: NPCs can greet at close range regardless of angle
- **Four Filter Modes**: Flexible AND/OR combinations (AngleOnly, DistanceOnly, Both, Either)

#### Critical Bug Fixes
- Fixed buffer overrun in all pattern scanners (crash prevention)
- Fixed unaligned memory access causing General Protection Faults
- Fixed illegal instruction crashes on pre-2013 CPUs
- Added AVX2 OS support verification (Windows 7 compatibility)
- Fixed race condition in global initialization
- Improved exception handling with separate handlers
- Added instruction cache flushing after code modifications
- Fixed RAX register corruption in jump trampoline (changed to R11)

#### Code Quality
- Comprehensive input validation
- Better const-correctness
- Enhanced error messages with exception diagnostics
- Extensive documentations

---

## Installation

### Requirements
- **Skyrim Special Edition 1.6.1170.0+** 
- **SKSE64 2.2.6.0+**

### Steps
1. Install SKSE64 if you haven't already
2. Download the latest release
3. Extract `to_your_face.dll` to `Data\SKSE\Plugins\`
4. (Optional) Copy `to_your_face.ini` to `Data\SKSE\Plugins\` and customize settings

---

## Configuration

The plugin is configured via `Data\SKSE\Plugins\to_your_face.ini`. See [to_your_face.ini](config/to_your_face.ini) for detailed documentation.

---

## How It Works

### Technical Overview

The plugin modifies Skyrim's runtime behavior through a sophisticated multi-phase injection process:

1. **Pattern Scanning** - Locates Skyrim's NPC comment function using byte pattern matching
2. **Code Injection** - Uses Xbyak to generate x64 assembly hook at runtime
3. **Angle & Distance Calculation** - Computes 3D angle and distance between player and NPC
4. **Comment Filtering** - Applies configured filter mode to allow/block comment

### The Hook Mechanism

When an NPC attempts to make a comment:

```
NPC triggers comment
  → Jump to our hook code
  → Call AllowComment(npc)
    → Check if player exists
    → Calculate 3D distance and angle
    → Check close range bypass if enabled
    → Apply filter mode (Angle/Distance/Both/Either)
    → Return true/false
  → Set result flag
  → Return to Skyrim code
  → Skyrim proceeds/blocks comment based on flag
```

---

## Troubleshooting

### Plugin Not Loading
1. Check SKSE64 log: `Documents\My Games\Skyrim Special Edition\SKSE\to_your_face.log`

### NPCs Still Comment When Not Facing
1. Increase `MaxDeviationAngle` in config (try 45 or 60 degrees)
2. Check that plugin is actually loaded (look for log file)
3. Ensure no other mods are overriding NPC behavior
4. Verify you're using the correct filter mode in config

---

## Compatibility

- **Game Version:** Skyrim SE 1.6.1170.0+ / Anniversary Edition
- **SKSE64:** Version 2.0.20+
- **VR Support:** Should work but untested
- **Other Mods:** Compatible with most mods
  - **May conflict with:** Mods that modify NPC greeting behavior
  - **Will conflict if:** Another mod hooks the exact same code location

### Binary Compatibility Check

The plugin includes a safety check to prevent conflicts. If installation fails with "Binary compatibility check failed", another mod has likely modified the same code region.

---

## Known Limitations

These are intentional design decisions:

- Only affects NPC "comment" dialogue (greetings, idle chatter)
- Does not affect quest dialogue or forced conversations
- Hook memory (256 bytes) is never freed - must stay resident for game lifetime
- Uses RWX memory permissions (required for runtime code generation)
- No thread synchronization (relies on read-only config and thread-local stack)

---

## Building from Source

### Prerequisites
- Visual Studio 2022
- CMake 3.22+
- vcpkg
- CommonLibSSE-NG

### Environment Setup
```powershell
$env:VCPKG_ROOT = "C:/dev/vcpkg"
$env:CommonLibSSEPath_NG = "C:/your/dev/CommonLibSSE-NG"
$env:CompiledPluginsPath = "C:/your/skyrim/Data/SKSE/Plugins/"
```

### Build Commands
```powershell
# Configure
cmake --preset vs2022-windows

# Build Release
cmake --build build --config Release
```

---

## Credits

**Original Mod Authors:**
- [underthesky](https://www.nexusmods.com/profile/underthesky?gameId=110) - Original "To Your Face" for Skyrim LE
- [xILARTH](https://www.nexusmods.com/profile/xILARTH?gameId=1704) - SE port
- [sfts](https://www.nexusmods.com/skyrimspecialedition/users/261389) - To Your Face SE - AE - VR

**Modernization & Development:**
- **Code Review & Improvements:** Claude Sonnet 4.5
- **SKSE Team:** For SKSE64 framework
- **Xbyak:** For runtime assembly generation library

---

## License & Permissions

Per the original author ([underthesky](https://www.nexusmods.com/skyrim/mods/87635)):

> "You are free to use the mod and its source in any form as long as you give full credit and link back to this page when you publish your modification or reupload the mod. I would also appreciate if you let me know when you do so, but that is not a requirement."

**Full credit to:**
- [underthesky (original author)](<https://www.nexusmods.com/profile/xILARTH?gameId=1704)>)
- [xILARTH (SE port)](https://www.nexusmods.com/profile/xILARTH?gameId=1704)
- [sfts (SE-AE-VR release)](https://www.nexusmods.com/skyrimspecialedition/users/261389)
- [SKSE team](https://www.nexusmods.com/skyrimspecialedition/mods/30379)

---

## Contributing

Suggestions and bug reports welcome! Open an issue or pull request.

---
