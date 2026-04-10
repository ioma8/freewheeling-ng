#include "fweelin_signal.h"

#include <signal.h>
#include <unistd.h>

static fweelin_signal_write_fn g_test_writer = 0;
static fweelin_signal_exit_fn g_test_exiter = 0;
static void *g_test_ctx = 0;

static size_t append_text(char *buf, size_t bufsz, size_t pos, const char *text) {
  while (pos + 1 < bufsz && *text != '\0')
    buf[pos++] = *text++;
  if (bufsz > 0)
    buf[pos < bufsz ? pos : bufsz - 1] = '\0';
  return pos;
}

static const char *fatal_name(int sig) {
  switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    case SIGFPE: return "SIGFPE";
    default: return "SIGNAL";
  }
}

static const char *fatal_text(int sig) {
  switch (sig) {
    case SIGSEGV: return "Segmentation fault";
    case SIGBUS: return "Access to undefined memory object";
    case SIGILL: return "Illegal instruction";
    case SIGFPE: return "Erroneous arithmetic operation";
    default: return "Fatal signal received";
  }
}

static const char *info_text(int sig) {
  switch (sig) {
    case SIGUSR1: return ">>> User defined signal 1 (SIGUSR1) received <<<\n";
    case SIGUSR2: return ">>> User defined signal 2 (SIGUSR2) received <<<\n";
    default: return ">>> Signal received <<<\n";
  }
}

static void dispatch_write(const char *msg, size_t len) {
  if (g_test_writer != 0) {
    g_test_writer(msg, len, g_test_ctx);
    return;
  }

  while (len > 0) {
    ssize_t written = write(STDERR_FILENO, msg, len);
    if (written <= 0)
      break;
    msg += written;
    len -= (size_t) written;
  }
}

static void dispatch_exit(int code) {
  if (g_test_exiter != 0) {
    g_test_exiter(code, g_test_ctx);
    return;
  }
  _exit(code);
}

size_t fweelin_format_signal_message(int sig, char *buf, size_t bufsz) {
  size_t pos = 0;

  if (buf == 0 || bufsz == 0)
    return 0;

  pos = append_text(buf, bufsz, pos, ">>> FATAL ERROR: ");
  pos = append_text(buf, bufsz, pos, fatal_text(sig));
  pos = append_text(buf, bufsz, pos, " (");
  pos = append_text(buf, bufsz, pos, fatal_name(sig));
  pos = append_text(buf, bufsz, pos, ") occurred! <<<\n");
  return pos;
}

void fweelin_log_nonfatal_signal(int sig) {
  const char *msg = info_text(sig);
  size_t len = 0;
  while (msg[len] != '\0')
    len++;
  dispatch_write(msg, len);
}

void fweelin_fatal_signal_handler(int sig) {
  static const char deferred_msg[] =
    "Stack trace generation is deferred to a safe context.\n";
  char buf[160];
  size_t len = fweelin_format_signal_message(sig, buf, sizeof(buf));

  dispatch_write(buf, len);
  dispatch_write(deferred_msg, sizeof(deferred_msg) - 1);
  dispatch_exit(128 + sig);
}

void fweelin_set_signal_test_hooks(fweelin_signal_write_fn writer,
                                   fweelin_signal_exit_fn exiter,
                                   void *ctx) {
  g_test_writer = writer;
  g_test_exiter = exiter;
  g_test_ctx = ctx;
}

void fweelin_clear_signal_test_hooks(void) {
  g_test_writer = 0;
  g_test_exiter = 0;
  g_test_ctx = 0;
}
