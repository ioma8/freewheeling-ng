#include "stacktrace.h"

#include <assert.h>
#include <string.h>

int main(void) {
  struct {
    char dst[32];
    unsigned char guard;
  } short_buffer;
  char long_name[256];
  char full_buffer[512];

  memset(long_name, 'A', sizeof(long_name) - 1);
  long_name[sizeof(long_name) - 1] = '\0';

  memset(&short_buffer, 'q', sizeof(short_buffer));
  short_buffer.guard = 0x3c;

  assert(stacktrace_format_symbol_entry(short_buffer.dst,
                                        sizeof(short_buffer.dst), 7,
                                        0x12345678UL, long_name, 0x44UL,
                                        'T') == -1);
  assert(short_buffer.dst[sizeof(short_buffer.dst) - 1] == '\0');
  assert(short_buffer.guard == 0x3c);

  assert(stacktrace_format_symbol_entry(full_buffer, sizeof(full_buffer), 3,
                                        0x1234UL, "short_name", 0x20UL,
                                        't') == 0);
  assert(strstr(full_buffer, "short_name") != 0);
  assert(strstr(full_buffer, "0x00001234") != 0);

  return 0;
}
