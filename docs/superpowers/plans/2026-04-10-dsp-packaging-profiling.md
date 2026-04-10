# DSP Split, Dependency Packaging, and Profiling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split `src/fweelin_core_dsp.cc` into safer units, make macOS third-party dependencies reproducible, and optimize DSP only from measured profiling data.

**Architecture:** The work lands as three independent subprojects. First split DSP code along existing responsibility boundaries without changing behavior. Then make the macOS dependency toolchain reproducible through a repo-managed prefix and shared local/CI scripts. Finally add profiling instrumentation and use it to drive targeted DSP optimizations.

**Tech Stack:** C++20, Xcode/macOS app build, shell scripts, GitHub Actions, Homebrew-compatible source dependency builds

---

### Task 1: Write and verify the DSP split boundary

**Files:**
- Create: `src/fweelin_core_dsp_audio_buffers.cc`
- Create: `src/fweelin_core_dsp_pulse.cc`
- Create: `src/fweelin_core_dsp_root.cc`
- Modify: `src/fweelin_core_dsp.cc`
- Modify: `MacOSX/fweelin.xcodeproj/project.pbxproj`
- Test: `scripts/build-macos.sh`

- [ ] **Step 1: Move `AudioLevel`, `AudioBuffers`, and `InputSettings` definitions into `src/fweelin_core_dsp_audio_buffers.cc`**

- [ ] **Step 2: Run build to verify the first move**

Run: `zsh scripts/build-macos.sh`
Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 3: Move `Pulse` definitions into `src/fweelin_core_dsp_pulse.cc`**

- [ ] **Step 4: Run build to verify the pulse split**

Run: `zsh scripts/build-macos.sh`
Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 5: Move `RootProcessor` definitions into `src/fweelin_core_dsp_root.cc`**

- [ ] **Step 6: Wire new translation units into `project.pbxproj`**

- [ ] **Step 7: Run build and regression tests**

Run:
- `zsh scripts/build-macos.sh`
- `zsh scripts/test-macos.sh`
- `git diff --check`

Expected:
- build succeeds
- tests succeed
- diff check is clean

- [ ] **Step 8: Commit**

```bash
git add src/fweelin_core_dsp.cc src/fweelin_core_dsp_audio_buffers.cc src/fweelin_core_dsp_pulse.cc src/fweelin_core_dsp_root.cc MacOSX/fweelin.xcodeproj/project.pbxproj
git commit -m "Split core DSP translation units"
```

### Task 2: Introduce reproducible macOS dependency packaging

**Files:**
- Create: `third_party/macos/README.md`
- Create: `third_party/macos/build-deps.sh`
- Create: `third_party/macos/env.sh`
- Modify: `scripts/bootstrap-macos.sh`
- Modify: `scripts/build-macos.sh`
- Modify: `scripts/test-macos.sh`
- Modify: `.github/workflows/macos-build.yml`
- Modify: `README.md`

- [ ] **Step 1: Define the repo-managed macOS dependency prefix layout**

- [ ] **Step 2: Add dependency build script and env helper under `third_party/macos/`**

- [ ] **Step 3: Update local bootstrap/build/test scripts to prefer the repo-managed prefix**

- [ ] **Step 4: Update CI to build and use the same dependency prefix**

- [ ] **Step 5: Document the dependency flow in `README.md`**

- [ ] **Step 6: Run local verification**

Run:
- `zsh scripts/bootstrap-macos.sh`
- `zsh scripts/build-macos.sh`
- `zsh scripts/test-macos.sh`

Expected:
- dependency bootstrap completes or clearly reports the first missing prerequisite
- build succeeds using the repo-managed dependency prefix
- tests succeed

- [ ] **Step 7: Commit**

```bash
git add third_party/macos scripts/bootstrap-macos.sh scripts/build-macos.sh scripts/test-macos.sh .github/workflows/macos-build.yml README.md
git commit -m "Add reproducible macOS dependency packaging"
```

### Task 3: Add measured DSP profiling and optimize from findings

**Files:**
- Create: `scripts/profile-dsp-macos.sh`
- Create: `docs/profiling/2026-04-10-dsp-baseline.md`
- Modify: `src/fweelin_core_dsp_root.cc`
- Modify: `src/fweelin_core_dsp_pulse.cc`
- Modify: `src/fweelin_audioio.cc` if needed for coarse timing hooks

- [ ] **Step 1: Add optional coarse profiling timers around root DSP boundaries**

- [ ] **Step 2: Add a profiling script that runs a controlled build/profile command**

- [ ] **Step 3: Record a baseline profiling report in `docs/profiling/2026-04-10-dsp-baseline.md`**

- [ ] **Step 4: Identify the top measured hotspot and implement the smallest useful optimization**

- [ ] **Step 5: Re-run profiling and document the delta**

- [ ] **Step 6: Run full verification**

Run:
- `zsh scripts/build-macos.sh`
- `zsh scripts/test-macos.sh`
- `zsh scripts/profile-dsp-macos.sh`

Expected:
- build succeeds
- tests succeed
- profiling script produces usable timing output
- optimization is justified by measured results

- [ ] **Step 7: Commit**

```bash
git add scripts/profile-dsp-macos.sh docs/profiling/2026-04-10-dsp-baseline.md src/fweelin_core_dsp_root.cc src/fweelin_core_dsp_pulse.cc src/fweelin_audioio.cc
git commit -m "Profile and optimize DSP hot paths"
```
