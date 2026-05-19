# Repository Overview

This repository is a preservation-and-port fork of **Freewheeling**, an older
live-looping application. The core application logic remains in the legacy
C/C++ code under `src/`, while the macOS-specific wrapper lives under
`MacOSX/`. The goal of the fork is not to redesign the application, but to keep
the original engine and configuration model working on modern macOS systems.

## Layout

- `src/`
  Main application code. This is where the runtime entry point, audio engine,
  MIDI handling, SDL-based UI/input, DSP pipeline, event system, persistence,
  browser, and loop/block management live.
- `MacOSX/`
  Native macOS wrapper and Xcode project. This layer handles Cocoa startup,
  app integration, and packaging while delegating most behavior back to the
  original engine.
- `scripts/`
  Checked-in helper scripts for bootstrapping dependencies, building the app,
  packaging dylibs, running the macOS app, and executing the local regression
  path.
- `tests/`
  Small regression-focused native test suite. Coverage is targeted at safety
  and startup/runtime behavior rather than broad unit testing.
- `data/`
  Runtime assets such as fonts and bundled data files.
- `examples/`
  Sample configuration and usage material.
- `third_party/macos/`
  macOS-specific third-party notes and compatibility support.

## Runtime Structure

The main executable entry point is `src/fweelin.cc`. It installs signal
handlers, initializes diagnostics, constructs a `Fweelin` instance, then calls
into the main setup and run flow.

The central object model is concentrated in `fweelin_core.*` and related
subsystems. The codebase has the shape of a classic older native application:
large internal modules, project-specific infrastructure, and broad ownership
from a single core instead of many small libraries.

On macOS, `MacOSX/FweelinMac.mm` and related Cocoa bootstrap files bridge the
original engine into a native app shell. The port preserves the original UI and
event model rather than replacing it with a native Aqua interface.

## Build And Verification

The supported macOS build path is script-driven:

- `scripts/bootstrap-macos.sh`
  Installs Homebrew dependencies.
- `scripts/build-macos.sh`
  Builds `MacOSX/fweelin.xcodeproj` and packages dylibs into the app bundle.
- `scripts/test-macos.sh`
  Verifies the packaged binary is not still linked against Homebrew paths and
  runs runtime regression checks.

GitHub Actions mirrors that same local flow through the workflow in
`.github/workflows/`.

## Summary

In practical terms, this is a working macOS wrapper around a legacy
Linux-era SDL audio application. Most of the engineering value in the fork is
in startup stability, audio/runtime safety, packaging, and maintaining a usable
native macOS build without rewriting the original engine.
