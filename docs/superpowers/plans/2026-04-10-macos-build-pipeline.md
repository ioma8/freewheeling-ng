# macOS Build Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add reproducible local macOS setup/build/test scripts and a matching GitHub Actions workflow that uses those same entrypoints.

**Architecture:** Keep Homebrew as the dependency source of truth, but move all setup/build/test behavior into checked-in shell scripts. CI should invoke those scripts directly rather than duplicating setup logic.

**Tech Stack:** zsh shell scripts, Xcode/xcodebuild, Homebrew, GitHub Actions

---

### Task 1: Add macOS bootstrap/build/test entrypoints

**Files:**
- Create: `scripts/bootstrap-macos.sh`
- Create: `scripts/build-macos.sh`
- Create: `scripts/test-macos.sh`
- Modify: `scripts/run-runtime-regression-tests.sh`

- [ ] **Step 1: Add a bootstrap script**
- [ ] **Step 2: Add a build script**
- [ ] **Step 3: Add a test script**
- [ ] **Step 4: Update the runtime regression harness to use the project C++ standard/tooling more consistently**
- [ ] **Step 5: Run the build script**
  Run: `zsh scripts/build-macos.sh`
  Expected: `** BUILD SUCCEEDED **`
- [ ] **Step 6: Run the test script**
  Run: `zsh scripts/test-macos.sh`
  Expected: all local regression checks pass

### Task 2: Add GitHub Actions macOS CI

**Files:**
- Create: `.github/workflows/macos-build.yml`

- [ ] **Step 1: Add the workflow**
- [ ] **Step 2: Make CI call the checked-in scripts directly**
- [ ] **Step 3: Validate workflow syntax locally as far as practical**

### Task 3: Document the supported flow

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update README to point to the new scripts**
- [ ] **Step 2: Document the CI path and dependency assumptions**
- [ ] **Step 3: Run a final build and test pass**
  Run: `zsh scripts/build-macos.sh && zsh scripts/test-macos.sh`
  Expected: both succeed
