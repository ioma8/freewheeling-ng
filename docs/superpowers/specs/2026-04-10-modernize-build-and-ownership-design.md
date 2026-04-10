# Build And Ownership Modernization Design

## Goal

Modernize the macOS fork so it builds and runs with a stricter toolchain, SDL2-only linkage, explicit C++14, a realistic deployment target, and clearer top-level ownership in bootstrap/runtime code.

## Scope

This design covers five concrete changes:

1. Remove SDL 1.2 linkage and keep the app building and running on SDL2 only.
2. Remove `-fpermissive`.
3. Set an explicit C++14 language standard.
4. Refactor top-level ownership in `Fweelin` and config/bootstrap paths to use `std::unique_ptr`.
5. Lower the deployment target from `26.0` to a real support floor.

## Current State

- The Xcode target links both SDL 1.2 and SDL2 in [project.pbxproj](/Users/jakubkolcar/customs/freewheeling/MacOSX/fweelin.xcodeproj/project.pbxproj).
- The project relies on `-fpermissive`, which hides real type and conversion problems instead of forcing them to be fixed.
- The project does not explicitly declare a C++ standard in Xcode settings.
- `Fweelin` still owns many top-level components as raw pointers in [fweelin_core.h](/Users/jakubkolcar/customs/freewheeling/src/fweelin_core.h).
- The current deployment target is `26.0`, which is not a practical compatibility baseline.

## Architecture

The modernization will stay incremental and non-invasive to runtime behavior.

- Build-system modernization happens first so compiler diagnostics become authoritative.
- SDL modernization is limited to dependency/link cleanup unless source-level SDL1 APIs are found.
- Ownership changes are limited to top-level bootstrap-owned components first; RT-sensitive processor/event/memory internals stay untouched in this pass.
- Verification stays macOS-focused: clean Xcode builds plus existing regression scripts.

## Design Decisions

### SDL

Treat SDL2 as the only supported SDL runtime for the macOS fork. Remove `-lSDL` and `-lSDLmain` from the Xcode project and fix any remaining compile or runtime assumptions if they surface.

### Compiler Strictness

Remove `-fpermissive` and explicitly set `CLANG_CXX_LANGUAGE_STANDARD = gnu++14`. Fix resulting errors in source rather than loosening flags elsewhere.

### Deployment Target

Set `MACOSX_DEPLOYMENT_TARGET = 11.0`. That is a realistic Apple Silicon-era baseline and aligns with modern Homebrew/Xcode expectations better than `26.0`.

### Ownership

Introduce `std::unique_ptr` for top-level `Fweelin`-owned components that are created/destroyed during startup and rollback:

- `MemoryManager`
- `BlockManager`
- `EventManager`
- `RootProcessor`
- `TriggerMap`
- `LoopManager`
- `AudioIO`
- `MidiIO`
- `SDLIO`
- `VideoIO`
- `FloConfig`
- `HardwareMixerInterface`
- optional streamers and bootstrap helpers where safe

For fixed browser slots and input streamer arrays, use owning STL containers instead of naked pointer arrays where that does not change external interfaces.

## Risk Areas

- Some legacy constructors or APIs may still require raw pointers. Use `.get()` at call boundaries instead of rewriting entire subsystems.
- `RollbackSetup()` and normal teardown must remain behaviorally equivalent after ownership conversion.
- SDL2-only linkage must be validated with an actual app launch, not just compile success.

## Verification

- `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release clean build`
- `zsh scripts/run-runtime-regression-tests.sh`
- short smoke launch of `MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin`

