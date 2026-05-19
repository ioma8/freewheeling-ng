#include "stacktrace.h"

#include <assert.h>
#include <string.h>

int main(void) {
  struct {
    char dst[8];
    unsigned char guard;
  } buffer;

  memset(&buffer, 'x', sizeof(buffer));
  buffer.guard = 0x5a;

  assert(stacktrace_copy_symbol_name(buffer.dst, sizeof(buffer.dst), "abc") ==
         0);
  assert(strcmp(buffer.dst, "abc") == 0);
  assert(buffer.guard == 0x5a);

  memset(&buffer, 'y', sizeof(buffer));
  buffer.guard = 0x6b;

  assert(stacktrace_copy_symbol_name(buffer.dst, sizeof(buffer.dst),
                                     "123456789") == 0);
  assert(strcmp(buffer.dst, "1234567") == 0);
  assert(buffer.dst[sizeof(buffer.dst) - 1] == '\0');
  assert(buffer.guard == 0x6b);

  return 0;
}
