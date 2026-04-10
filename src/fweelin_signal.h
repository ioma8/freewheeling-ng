#ifndef __FWEELIN_SIGNAL_H
#define __FWEELIN_SIGNAL_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*fweelin_signal_write_fn)(const char *msg, size_t len, void *ctx);
typedef void (*fweelin_signal_exit_fn)(int code, void *ctx);

size_t fweelin_format_signal_message(int sig, char *buf, size_t bufsz);
void fweelin_log_nonfatal_signal(int sig);
void fweelin_fatal_signal_handler(int sig);

void fweelin_set_signal_test_hooks(fweelin_signal_write_fn writer,
                                   fweelin_signal_exit_fn exiter,
                                   void *ctx);
void fweelin_clear_signal_test_hooks(void);

#if defined(__cplusplus)
}
#endif

#endif
