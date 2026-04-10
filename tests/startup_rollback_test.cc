#include "fweelin_startup_guard.h"
#include <assert.h>

static int steps[8];
static int step_count = 0;

static void mark_step(void *ctx, int id) {
  (void)ctx;
  steps[step_count++] = id;
}

int main() {
  FweelinStartupGuard rollback_guard;
  rollback_guard.Push(mark_step, (void *)0, 1);
  rollback_guard.Push(mark_step, (void *)0, 2);
  rollback_guard.Rollback();
  assert(step_count == 2);
  assert(steps[0] == 2);
  assert(steps[1] == 1);

  step_count = 0;

  FweelinStartupGuard released_guard;
  released_guard.Push(mark_step, (void *)0, 3);
  released_guard.Push(mark_step, (void *)0, 4);
  released_guard.Release();
  released_guard.Rollback();
  assert(step_count == 0);

  return 0;
}
