# Build And Ownership Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernize the macOS fork to SDL2-only, explicit C++14, strict compilation without `-fpermissive`, practical deployment targeting, and RAII top-level ownership.

**Architecture:** Tackle build-system strictness first so diagnostics are trustworthy, then remove SDL 1.2 linkage, then convert top-level ownership in `Fweelin` and bootstrap/config code to `std::unique_ptr` and STL containers without changing subsystem internals.

**Tech Stack:** Xcode, Clang, C++14, SDL2, Cocoa/Objective-C++, libxml2, CoreAudio/CoreMIDI

---

### Task 1: Modernize Xcode Build Settings

**Files:**
- Modify: `MacOSX/fweelin.xcodeproj/project.pbxproj`

- [ ] Remove `-fpermissive` from Debug and Release `OTHER_CFLAGS`
- [ ] Remove `-lSDL` and `-lSDLmain` from Debug and Release `OTHER_LDFLAGS`
- [ ] Set `CLANG_CXX_LANGUAGE_STANDARD = gnu++14`
- [ ] Set `MACOSX_DEPLOYMENT_TARGET = 11.0`
- [ ] Run: `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

### Task 2: Repair Any Strict-Compiler Or SDL2 Breakage

**Files:**
- Modify: whichever source files fail after Task 1

- [ ] Fix compile failures introduced by removing `-fpermissive`
- [ ] Fix any remaining SDL1-era assumptions if the linker/compiler surfaces them
- [ ] Run: `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

### Task 3: Convert Top-Level Fweelin Ownership To RAII

**Files:**
- Modify: `src/fweelin_core.h`
- Modify: `src/fweelin_core.cc`

- [ ] Add `<memory>` and replace selected top-level raw owning pointers in `Fweelin` with `std::unique_ptr`
- [ ] Convert browser/input-streamer ownership to STL-backed owning structures where safe
- [ ] Update call sites to use `.get()` only at interface boundaries
- [ ] Keep getters stable so the rest of the codebase does not need invasive rewrites
- [ ] Run: `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

### Task 4: Simplify Bootstrap And Rollback With RAII

**Files:**
- Modify: `src/fweelin_core.cc`
- Modify: `src/fweelin_config.h`
- Modify: `src/fweelin_config.cc`

- [ ] Remove now-redundant manual deletes/nulling from setup/rollback paths where unique ownership already guarantees cleanup
- [ ] Keep failure rollback behavior equivalent
- [ ] Run: `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

### Task 5: Full Verification

**Files:**
- No source changes required unless verification fails

- [ ] Run: `zsh scripts/run-runtime-regression-tests.sh`
- [ ] Run: `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release clean build`
- [ ] Run a short smoke launch of `MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin`
- [ ] Summarize any remaining risk that was intentionally left out of scope
