# Runtime Stability Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make FreeWheeling significantly safer under startup failure, fatal-signal handling, and realtime processor lifecycle changes without changing its external behavior.

**Architecture:** Keep the existing C++03-style codebase intact and improve it by extracting small, testable seams around the risky areas instead of attempting broad modernization. Introduce an explicit startup rollback helper for partial initialization, replace the current signal handler with an async-signal-safe fatal path plus optional postmortem helper, and make the RT audio thread the sole owner of the processor list by converting non-RT delete/search operations into queued commands.

**Tech Stack:** C++ / Objective-C++, Xcode/macOS build, CoreAudio, SDL 1.2/2 shims, small native regression binaries compiled with `clang++`/`cc`

---

## File Structure

**New files**
- `docs/superpowers/plans/2026-04-10-runtime-stability-hardening.md`
  Plan document.
- `scripts/run-runtime-regression-tests.sh`
  One-command runner for the new native regression binaries.
- `tests/startup_rollback_test.cc`
  Characterization tests for partial startup teardown and owned-memory cleanup.
- `tests/signal_handler_test.c`
  Tests for async-signal-safe crash helper formatting and allowed-code-path behavior.
- `tests/root_processor_queue_test.cc`
  Tests for processor command queue behavior and list ownership rules.
- `src/fweelin_startup_guard.h`
  Small rollback helper used only during `Fweelin::setup()`.
- `src/fweelin_startup_guard.cc`
  Implementation of the rollback helper and any small ownership utilities.
- `src/fweelin_signal.h`
  Small interface for fatal-signal-safe reporting and optional deferred stacktrace launch.
- `src/fweelin_signal.c`
  C implementation restricted to async-signal-safe operations where needed.

**Modified files**
- `src/fweelin_core.cc`
  Replace early-return startup failures with staged rollback; fix startup-owned leaks.
- `src/fweelin_core.h`
  Add any tiny declarations needed by startup cleanup; keep ownership explicit.
- `src/fweelin.cc`
  Replace the current signal handler wiring with the new minimal fatal path.
- `src/stacktrace.c`
  Restrict stacktrace launching to non-signal-handler contexts only.
- `src/fweelin_core_dsp.h`
  Replace `protect_plist`-driven cross-thread mutation with queue-only command declarations.
- `src/fweelin_core_dsp.cc`
  Make the RT thread the only mutator of the processor list; remove non-RT list scanning.
- `MacOSX/fweelin.xcodeproj/project.pbxproj`
  Add the new helper files to the macOS build targets as part of this work.

**Verification files**
- `README.md`
  Only touch if the runtime regression test command or crash-handling behavior needs documenting.

---

### Task 1: Add a Minimal Native Regression Harness

**Files:**
- Create: `scripts/run-runtime-regression-tests.sh`
- Create: `tests/startup_rollback_test.cc`
- Create: `tests/signal_handler_test.c`
- Create: `tests/root_processor_queue_test.cc`

- [ ] **Step 1: Write the failing startup rollback test**

```cpp
// tests/startup_rollback_test.cc
#include "fweelin_startup_guard.h"
#include <assert.h>

static int steps[8];
static int step_count = 0;

static void mark_step(void *ctx, int id) {
  (void)ctx;
  steps[step_count++] = id;
}

int main() {
  FweelinStartupGuard guard;
  guard.Push(mark_step, (void *)0, 1);
  guard.Push(mark_step, (void *)0, 2);
  guard.Rollback();
  assert(step_count == 2);
  assert(steps[0] == 2);
  assert(steps[1] == 1);
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `clang++ -std=gnu++03 -I./src tests/startup_rollback_test.cc src/fweelin_startup_guard.cc -o tests/startup_rollback_test`
Expected: FAIL with missing `fweelin_startup_guard.h` / `fweelin_startup_guard.cc`

- [ ] **Step 3: Add the rest of the empty harness entry points**

```sh
# scripts/run-runtime-regression-tests.sh
#!/bin/zsh
set -euo pipefail

clang++ -std=gnu++03 -I./src tests/startup_rollback_test.cc src/fweelin_startup_guard.cc -o tests/startup_rollback_test
cc -I./src tests/signal_handler_test.c src/fweelin_signal.c -o tests/signal_handler_test
clang++ -std=gnu++03 -I./src tests/root_processor_queue_test.cc -o tests/root_processor_queue_test

./tests/startup_rollback_test
./tests/signal_handler_test
./tests/root_processor_queue_test
```

- [ ] **Step 4: Run the harness to verify all three binaries fail for the right reasons**

Run: `zsh scripts/run-runtime-regression-tests.sh`
Expected: FAIL because the startup guard and signal helper are not implemented yet, and the root processor queue test uses placeholder symbols that do not exist.

- [ ] **Step 5: Commit**

```bash
git add scripts/run-runtime-regression-tests.sh tests/startup_rollback_test.cc tests/signal_handler_test.c tests/root_processor_queue_test.cc
git commit -m "test: add native regression harness for runtime hardening"
```

### Task 2: Make `setup()` Failures Roll Back Cleanly

**Files:**
- Create: `src/fweelin_startup_guard.h`
- Create: `src/fweelin_startup_guard.cc`
- Modify: `src/fweelin_core.cc`
- Modify: `src/fweelin_core.h`
- Test: `tests/startup_rollback_test.cc`

- [ ] **Step 1: Expand the failing startup rollback test to cover release semantics**

```cpp
int main() {
  FweelinStartupGuard guard;
  guard.Push(mark_step, (void *)0, 1);
  guard.Push(mark_step, (void *)0, 2);
  guard.Release();
  guard.Rollback();
  assert(step_count == 0);
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `clang++ -std=gnu++03 -I./src tests/startup_rollback_test.cc src/fweelin_startup_guard.cc -o tests/startup_rollback_test && ./tests/startup_rollback_test`
Expected: FAIL with missing implementation or wrong rollback order.

- [ ] **Step 3: Implement the minimal startup guard**

```cpp
// src/fweelin_startup_guard.h
typedef void (*FweelinRollbackFn)(void *ctx, int tag);

class FweelinStartupGuard {
public:
  FweelinStartupGuard();
  void Push(FweelinRollbackFn fn, void *ctx, int tag);
  void Release();
  void Rollback();
private:
  struct Entry { FweelinRollbackFn fn; void *ctx; int tag; };
  Entry entries[64];
  int count;
  int released;
};
```

- [ ] **Step 4: Integrate the guard into `Fweelin::setup()`**

Implementation requirements:
- Initialize all owned pointers to `0` before first use.
- Register rollback steps immediately after each successful acquisition/start.
- Replace early `return 1` failure exits after partial startup with `guard.Rollback(); return 1;`
- Build an explicit ownership/rollback map for every startup-acquired resource and every later hard-fail exit in `Fweelin::setup()`. At minimum cover: SDL/RT thread init, `mmg`, `cfg`, `emg`, `vid`, `audio`, `abufs`, `iset`, `browsers`, `bmg`, `rp`, `audiomem`, `amrec`, peaks/avgs, `tmap`, `loopmgr`, `snaps`, `sdlio`, `midi`, `hmix`, optional `osc`, optional `fluidp`, `cfg->Start()`, `audio->activate(rp)`, and file streamers.
- Fix the startup-owned leak called out near `audiomem` / peaks / avgs` by making ownership explicit and reclaimed on teardown and rollback.
- Keep `go()` teardown logic intact; `setup()` rollback should only handle partially initialized state.

- [ ] **Step 5: Run focused verification**

Run:
- `clang++ -std=gnu++03 -I./src tests/startup_rollback_test.cc src/fweelin_startup_guard.cc -o tests/startup_rollback_test && ./tests/startup_rollback_test`
- `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

Expected:
- startup guard test passes
- macOS app build succeeds

- [ ] **Step 6: Commit**

```bash
git add src/fweelin_startup_guard.h src/fweelin_startup_guard.cc src/fweelin_core.h src/fweelin_core.cc tests/startup_rollback_test.cc
git commit -m "fix: roll back partial startup safely"
```

### Task 3: Replace Fatal Signal Handling with an Async-Safe Path

**Files:**
- Create: `src/fweelin_signal.h`
- Create: `src/fweelin_signal.c`
- Modify: `src/fweelin.cc`
- Modify: `src/stacktrace.c`
- Test: `tests/signal_handler_test.c`

- [ ] **Step 1: Write the failing signal helper test**

```c
// tests/signal_handler_test.c
#include "fweelin_signal.h"
#include <assert.h>
#include <signal.h>
#include <string.h>

int main(void) {
  char buf[128];
  size_t n = fweelin_format_signal_message(SIGSEGV, buf, sizeof(buf));
  assert(n > 0);
  assert(strstr(buf, "SIGSEGV") != 0);
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cc -I./src tests/signal_handler_test.c src/fweelin_signal.c -o tests/signal_handler_test`
Expected: FAIL because `fweelin_signal.h` / `fweelin_signal.c` do not exist yet.

- [ ] **Step 3: Implement a minimal async-safe signal helper**

```c
// src/fweelin_signal.h
size_t fweelin_format_signal_message(int sig, char *buf, size_t bufsz);
void fweelin_fatal_signal_handler(int sig);
```

Implementation requirements:
- In the signal handler, use only async-signal-safe operations (`write`, `_exit`, `kill` only if truly necessary).
- Do not call `printf`, `snprintf`, `sleep`, `malloc`, `fork`, or `waitpid` from the handler.
- Preserve nonfatal signal behavior explicitly:
  - `SIGINT` remains a clean exit request path handled outside the fatal helper.
  - `SIGUSR1` and `SIGUSR2` keep their current nonfatal semantics or are intentionally dropped with an explicit rationale in code comments.
- Move any optional stacktrace launching behind a non-signal entry point so `stacktrace.c` is never entered from a fatal signal context. The fatal handler may only record minimal postmortem intent through async-safe state, and any richer postmortem action must run later from normal process code or an external helper.

- [ ] **Step 4: Wire `main()` to the new handler**

Implementation requirements:
- Replace the current `signal_handler` in [src/fweelin.cc](/Users/jakubkolcar/customs/freewheeling/src/fweelin.cc)
- Preserve the existing signal registration set
- Keep user-visible fatal messaging, but emit it through preformatted fixed buffers
- Add a test seam for the real fatal path, for example injected write/exit callbacks or a small internal helper, so the verification covers the same logic as `fweelin_fatal_signal_handler()` without actually terminating the test process.

- [ ] **Step 5: Run focused verification**

Run:
- `cc -I./src tests/signal_handler_test.c src/fweelin_signal.c -o tests/signal_handler_test && ./tests/signal_handler_test`
- `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

Expected:
- signal helper test passes
- macOS app build succeeds

- [ ] **Step 6: Commit**

```bash
git add src/fweelin_signal.h src/fweelin_signal.c src/fweelin.cc src/stacktrace.c tests/signal_handler_test.c
git commit -m "fix: make fatal signal handling async-safe"
```

### Task 4: Make the RT Thread the Sole Owner of the Processor List

**Files:**
- Modify: `src/fweelin_core_dsp.h`
- Modify: `src/fweelin_core_dsp.cc`
- Test: `tests/root_processor_queue_test.cc`

- [ ] **Step 1: Write the failing processor queue test**

```cpp
// tests/root_processor_queue_test.cc
#include "fweelin_core_dsp.h"
#include <assert.h>

int main() {
  ProcessorCommandQueue queue;
  Processor proc_a(0, 0, 0, 0, 0);
  Processor proc_b(0, 0, 0, 0, 0);

  assert(queue.PendingCount() == 0);
  queue.EnqueueAdd(&proc_a, 1, 0);
  queue.EnqueueAdd(&proc_b, 2, 0);
  assert(queue.PendingCount() == 2);
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `clang++ -std=gnu++03 -I./src tests/root_processor_queue_test.cc src/fweelin_core_dsp.cc -o tests/root_processor_queue_test && ./tests/root_processor_queue_test`
Expected: FAIL because `ProcessorCommandQueue` does not exist yet.

- [ ] **Step 3: Extract a tiny queue/state-machine seam and implement minimal behavior**

Implementation requirements:
- Remove the non-RT scan of `firstchild` from `RootProcessor::DelChild()`
- Convert delete requests into queue commands keyed by processor pointer
- Make `UpdateProcessors()` in RT own all list insertion, status flips, unlinking, and cleanup-event emission
- Delete `protect_plist` entirely if possible; if not, reduce it to RT-local preprocessing state only, not cross-thread synchronization
- Define the synchronization model explicitly:
  - non-RT threads only enqueue commands and never read/mutate the processor list directly
  - RT applies the queued commands at a single well-defined point before traversal
  - preprocess/read-only passes may observe immutable processor state only and must not depend on list mutation exclusion flags
  - cleanup events are generated only when RT unlinks a processor and must be one-shot

Suggested shape:

```cpp
struct ProcessorCommand {
  enum Type { CMD_ADD, CMD_MARK_DELETE } type;
  Processor *processor;
  ProcessorItem *item;
  int processor_type;
  char silent;
};
```

- [ ] **Step 4: Replace the placeholder test with concrete queue behavior assertions**

Test requirements:
- add command preserves insertion order
- mark-delete command does not mutate the list until RT apply
- pending-delete processor is unlinked only by RT apply path
- cleanup event is emitted exactly once per deleted processor

- [ ] **Step 5: Run focused verification**

Run:
- `clang++ -std=gnu++03 -I./src tests/root_processor_queue_test.cc src/fweelin_core_dsp.cc -o tests/root_processor_queue_test && ./tests/root_processor_queue_test`
- `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`

Expected:
- root processor queue test passes
- macOS app build succeeds

- [ ] **Step 6: Commit**

```bash
git add src/fweelin_core_dsp.h src/fweelin_core_dsp.cc tests/root_processor_queue_test.cc
git commit -m "refactor: give RT thread sole ownership of processor list"
```

### Task 5: Final Verification and Documentation

**Files:**
- Modify: `README.md` (only if needed)
- Modify: `scripts/run-runtime-regression-tests.sh`

- [ ] **Step 1: Run all native regression tests**

Run: `zsh scripts/run-runtime-regression-tests.sh`
Expected: all three binaries exit `0`

- [ ] **Step 2: Run the full macOS build**

Run: `xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release build`
Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 3: Smoke-check startup and shutdown**

Run:
- `open MacOSX/build/Release/fweelin.app`
- exercise launch, brief idle, clean quit

Expected:
- app launches
- no startup hang
- no immediate shutdown warning spam tied to the new lifecycle changes

- [ ] **Step 4: Document the regression command if needed**

```md
Native runtime regressions:

```sh
zsh scripts/run-runtime-regression-tests.sh
```
```

- [ ] **Step 5: Commit**

```bash
git add scripts/run-runtime-regression-tests.sh README.md
git commit -m "docs: document runtime stability regression checks"
```

## Notes for the Implementer

- Keep changes small and verifiable. Build after every task, not just at the end.
- Do not modernize unrelated subsystems while doing this work.
- Prefer extracting tiny helpers over widening existing giant files further.
- If `MacOSX/fweelin.xcodeproj/project.pbxproj` must change, keep it limited to adding any new helper source files. Do not mix stylistic project-file edits into the same commit.
