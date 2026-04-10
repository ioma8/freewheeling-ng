# DSP Split, Dependency Packaging, and Profiling Design

## Goal

Improve the next three highest-value engineering areas in this fork:

1. split `src/fweelin_core_dsp.cc` into smaller, safer architectural units
2. make macOS dependency packaging reproducible instead of depending on ambient Homebrew state
3. add a measured DSP profiling workflow and optimize only the hotspots it exposes

The work lands as three sequential subprojects with independent verification.

## Current State

- `src/fweelin_core_dsp.cc` still mixes several responsibilities:
  - audio buffer helpers
  - pulse/sync logic
  - root processor orchestration
  - processor queue application
  - record/play/file streamer implementations
- macOS builds are scripted, but still link directly against Homebrew dylibs in `/opt/homebrew`
- the build is clean and functional, but dependency reproducibility and deployment-floor compatibility remain external to the repo
- recent performance work removed obvious callback overhead, so the next optimization pass should be measurement-driven inside DSP code rather than speculative

## Approach Options

### Option 1: Sequential split, packaging, and measured profiling

Split the DSP file along existing responsibility boundaries, then add a repo-managed dependency build/prefix, then add instrumentation and optimize measured hotspots.

Pros:
- lowest integration risk
- each subproject produces a usable intermediate state
- profiling happens after structural cleanup, so measurements are easier to interpret

Cons:
- takes longer than a one-shot refactor

Recommendation: use this option.

### Option 2: Packaging first, then DSP split, then profiling

This is valid, but the DSP split is currently the more immediate code health risk. Packaging is important, but the repo already builds today.

### Option 3: Profile first, then restructure around findings

Not recommended. The current `fweelin_core_dsp.cc` shape makes profile data harder to act on safely, and profiling before reproducible packaging means later measurements are still tied to local machine state.

## Design

### Subproject 1: `fweelin_core_dsp.cc` split

Use the existing file as the source of truth but move code into focused translation units without changing public behavior.

Planned structure:

- `src/fweelin_core_dsp.cc`
  - shared math/audio helper definitions kept only if still cross-cutting
- `src/fweelin_core_dsp_audio_buffers.cc`
  - `AudioLevel`
  - `AudioBuffers`
  - `InputSettings`
- `src/fweelin_core_dsp_pulse.cc`
  - `Pulse`
  - pulse-sync related helpers
- `src/fweelin_core_dsp_root.cc`
  - `RootProcessor`
  - processor-list update/apply logic
  - root routing/processchain orchestration

The first split stops there. `RecordProcessor`, `PlayProcessor`, and `FileStreamer` stay where they are unless the first pass proves too lopsided. The goal is a safe split, not maximum decomposition in one pass.

### Subproject 2: reproducible dependency packaging

Introduce a repo-managed third-party build prefix for macOS and make the build scripts prefer that prefix over `/opt/homebrew`.

Planned structure:

- `third_party/macos/` for source build manifests and build scripts
- `scripts/bootstrap-macos-deps.sh` or equivalent to fetch/build required libraries
- `scripts/build-macos.sh` updated to consume the repo-managed prefix
- CI updated to build/use the same dependency prefix

This subproject targets reproducible engineering builds first. It does not attempt notarized distribution packaging yet.

### Subproject 3: measured profiling pass

Add lightweight profiling instrumentation around DSP boundaries, run controlled measurements, then optimize only measured hotspots.

Initial profiling targets:

- `RootProcessor::process`
- `RootProcessor::processchain`
- `Pulse::process`
- routing copy/mix sections

Outputs:

- a profiling script or documented command
- one checked-in report of baseline findings
- targeted optimizations tied directly to measured hot paths

The profiling pass should prefer coarse timing around subsystems first, not per-sample tracing.

## Error Handling and Safety

- DSP split must preserve current public interfaces and build/test behavior after each move
- dependency packaging must fail clearly when prerequisites are missing or a third-party build breaks
- profiling must be optional and disabled in normal release builds unless explicitly requested

## Verification

Each subproject must run:

- `zsh scripts/build-macos.sh`
- `zsh scripts/test-macos.sh`
- any subproject-specific verification

Additional verification:

- Subproject 1: `git diff --check`, no behavior changes intended
- Subproject 2: CI and local scripts use the same dependency path
- Subproject 3: before/after measurements show what changed and why
