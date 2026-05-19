#include "stacktrace.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  unsigned long addr = 0;
  char type = '\0';
  char name[32];

  assert(stacktrace_parse_nm_symbol_line("1234abcd T short_name", &addr, &type,
                                         name, sizeof(name)) == 1);
  assert(addr == 0x1234abcdUL);
  assert(type == 'T');
  assert(strcmp(name, "short_name") == 0);

  char long_line[1024];
  char long_name[700];
  memset(long_name, 's', sizeof(long_name) - 1);
  long_name[sizeof(long_name) - 1] = '\0';
  snprintf(long_line, sizeof(long_line), "1 T %s", long_name);

  memset(name, 'x', sizeof(name));
  assert(stacktrace_parse_nm_symbol_line(long_line, &addr, &type, name,
                                         sizeof(name)) == 1);
  assert(name[sizeof(name) - 1] == '\0');
  assert(strlen(name) == sizeof(name) - 1);

  return 0;
}
