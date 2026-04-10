#include "fweelin_startup_guard.h"

FweelinStartupGuard::FweelinStartupGuard()
  : count(0), released(0) {
}

void FweelinStartupGuard::Push(FweelinRollbackFn fn, void *ctx, int tag) {
  if (released || fn == 0 || count >= MAX_ENTRIES)
    return;

  entries[count].fn = fn;
  entries[count].ctx = ctx;
  entries[count].tag = tag;
  count++;
}

void FweelinStartupGuard::Release() {
  released = 1;
  count = 0;
}

void FweelinStartupGuard::Rollback() {
  if (released)
    return;

  while (count > 0) {
    count--;
    entries[count].fn(entries[count].ctx, entries[count].tag);
  }

  released = 1;
}
