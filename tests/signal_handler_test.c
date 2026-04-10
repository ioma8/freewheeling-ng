#include "fweelin_signal.h"
#include <assert.h>
#include <signal.h>
#include <string.h>

struct TestHooks {
  char buffer[256];
  size_t used;
  int exit_code;
};

static void capture_write(const char *msg, size_t len, void *ctx) {
  struct TestHooks *hooks = (struct TestHooks *)ctx;
  if (hooks->used + len >= sizeof(hooks->buffer))
    len = sizeof(hooks->buffer) - hooks->used - 1;
  memcpy(hooks->buffer + hooks->used, msg, len);
  hooks->used += len;
  hooks->buffer[hooks->used] = '\0';
}

static void capture_exit(int code, void *ctx) {
  struct TestHooks *hooks = (struct TestHooks *)ctx;
  hooks->exit_code = code;
}

int main(void) {
  char buf[128];
  struct TestHooks hooks;
  memset(&hooks, 0, sizeof(hooks));

  size_t n = fweelin_format_signal_message(SIGSEGV, buf, sizeof(buf));
  assert(n > 0);
  assert(strstr(buf, "SIGSEGV") != 0);

  fweelin_set_signal_test_hooks(capture_write, capture_exit, &hooks);
  fweelin_fatal_signal_handler(SIGSEGV);
  fweelin_clear_signal_test_hooks();

  assert(strstr(hooks.buffer, "SIGSEGV") != 0);
  assert(strstr(hooks.buffer, "deferred to a safe context") != 0);
  assert(hooks.exit_code == 128 + SIGSEGV);
  return 0;
}
