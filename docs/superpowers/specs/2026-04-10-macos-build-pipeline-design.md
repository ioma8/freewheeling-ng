# macOS Build Pipeline Design

**Date:** 2026-04-10

**Scope:** Subproject 1 of the modernization effort. This covers a reproducible macOS dependency and build pipeline for both local development and CI. It does not yet cover dependency pruning or `fweelin_core.cc` decomposition.

## Goal

Make the macOS fork buildable in a repeatable way for contributors and CI from a clean machine, using checked-in scripts as the single source of truth for setup, build, and verification.

## Current State

The repository already builds on macOS, but the workflow is implicit:

- contributors are expected to infer required Homebrew packages from the README and Xcode project
- the build depends on local `/opt/homebrew` state
- there is no checked-in bootstrap script
- there is no checked-in top-level build/test entrypoint for macOS
- there is no CI workflow proving the macOS path continuously

This is workable for one machine but fragile for maintainers and outside contributors.

## Non-Goals

This subproject will not:

- vendor third-party dylibs into the repository
- produce notarized or release-distribution app bundles
- solve the Homebrew bottle deployment-target mismatch beyond documenting and surfacing it clearly
- change the application architecture outside what is needed for build reproducibility

## Recommended Approach

Use Homebrew-managed dependencies plus checked-in scripts, and have CI call exactly those scripts.

This is the standard approach for a C/C++ macOS open-source project that already depends on Homebrew paths. It keeps the repo free of binary payloads while making setup explicit and reviewable.

## Deliverables

### 1. Local bootstrap script

Add a script, expected path:

- `scripts/bootstrap-macos.sh`

Responsibilities:

- verify the host is macOS
- verify Xcode command line tools are installed
- verify Homebrew is available
- install the required formulas
- print any important caveats, especially around deployment-target expectations of Homebrew-provided dylibs

The script should be idempotent. Re-running it should be safe.

### 2. Local build script

Add a script, expected path:

- `scripts/build-macos.sh`

Responsibilities:

- run from repository root
- validate required tools
- call `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`
- fail clearly when prerequisites are missing

The script should not silently mutate unrelated project state.

### 3. Local verification script

Add a script, expected path:

- `scripts/test-macos.sh`

Responsibilities:

- build the project or require a successful build step first
- run the existing runtime regression harness
- optionally run an additional lightweight smoke check against the built app if that can be done deterministically

### 4. CI workflow

Add a GitHub Actions workflow, expected path:

- `.github/workflows/macos-build.yml`

Responsibilities:

- run on push and pull request
- use a macOS runner
- use Homebrew plus the checked-in bootstrap script
- use the checked-in build and verification scripts
- fail when build or verification fails

CI must not duplicate the setup logic inline when it can invoke the repo scripts instead.

### 5. Documentation

Update:

- `README.md`

Responsibilities:

- document the supported local flow using the new scripts
- document the intended CI path
- document the dependency assumptions
- mention the deployment-target caveat for Homebrew binaries

## File-Level Design

### `scripts/bootstrap-macos.sh`

Focused shell script with:

- strict shell mode
- a small helper for checking `brew`
- one explicit list of required formulas
- clear stdout messages

The dependency list should live in exactly one place inside the script or in a small adjacent config file if that keeps the script simpler. Do not scatter package names across multiple scripts.

### `scripts/build-macos.sh`

Focused shell script with:

- strict shell mode
- repo-root detection
- direct `xcodebuild` invocation

This should remain intentionally small. Build orchestration should not become a second build system.

### `scripts/test-macos.sh`

Focused shell script with:

- strict shell mode
- invocation of the existing `scripts/run-runtime-regression-tests.sh`
- optional smoke-run logic only if deterministic and fast

### `.github/workflows/macos-build.yml`

The workflow should:

- check out the repo
- invoke the bootstrap script
- invoke the build script
- invoke the test script

If caching is added, it should be minimal and understandable. Do not add a complex cache layer in the first version.

## Error Handling

Scripts should:

- exit non-zero on failure
- print short actionable errors
- avoid interactive prompts where possible

CI should:

- surface the exact script step that failed
- not contain hidden fallback behavior

## Testing Strategy

Validation for this subproject is:

1. local script execution from a normal developer shell
2. successful `xcodebuild` through the new script
3. successful regression harness through the new verification script
4. successful GitHub Actions run using those same scripts

## Risks

### Homebrew version drift

Homebrew formula updates can still change behavior over time. This design does not eliminate that; it makes the dependency contract explicit and easier to update.

### Deployment target mismatch warnings

The project target is now `macOS 11.0`, but Homebrew bottles on a given machine may be built for newer macOS versions. This design keeps that visible rather than hiding it. A later subproject can address true binary portability if needed.

### CI runner differences

GitHub Actions `macos-latest` may not match the local machine exactly. Using the same scripts keeps divergence limited to the environment rather than the procedure.

## Acceptance Criteria

This subproject is done when:

- a new contributor can follow `README.md` and build using the checked-in scripts
- the repository has a checked-in macOS bootstrap script
- the repository has checked-in macOS build and verification scripts
- GitHub Actions runs the macOS build and verification path successfully
- CI and local workflows use the same script entrypoints

## Follow-On Work

Once this subproject lands, subproject 2 can audit which of the installed libraries are truly required and remove unnecessary ones from:

- the Xcode project
- the bootstrap script
- the README
