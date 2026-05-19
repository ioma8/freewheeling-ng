#include "stacktrace.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char nm_cmd[32];
  char debugger_cmd[128];
  char quoted_nm_cmd[256];
  char quoted_debugger_cmd[512];
  char expected_debugger_cmd[512];
  char long_prog[128];
  char long_gdb_file[128];
  size_t i;

  memset(long_prog, 'p', sizeof(long_prog) - 1);
  long_prog[sizeof(long_prog) - 1] = '\0';
  memset(long_gdb_file, 'g', sizeof(long_gdb_file) - 1);
  long_gdb_file[sizeof(long_gdb_file) - 1] = '\0';

  assert(stacktrace_build_nm_command(nm_cmd, sizeof(nm_cmd), 1,
                                     long_prog) == -1);
  assert(nm_cmd[sizeof(nm_cmd) - 1] == '\0');

  assert(stacktrace_build_nm_command(nm_cmd, sizeof(nm_cmd), 0,
                                     "/tmp/fweelin") == 0);
  assert(strcmp(nm_cmd, "nm -B '/tmp/fweelin'") == 0);

  assert(stacktrace_build_nm_command(
             quoted_nm_cmd, sizeof(quoted_nm_cmd), 0,
             "/tmp/fweelin';touch /tmp/pwned #") == 0);
  assert(strcmp(quoted_nm_cmd,
                "nm -B '/tmp/fweelin'\"'\"';touch /tmp/pwned #'") == 0);

  assert(stacktrace_build_debugger_command(
             debugger_cmd, sizeof(debugger_cmd), "/tmp/fweelin",
             long_gdb_file) == -1);
  assert(debugger_cmd[sizeof(debugger_cmd) - 1] == '\0');

  assert(stacktrace_build_debugger_command(
             debugger_cmd, sizeof(debugger_cmd), "/tmp/fweelin",
             "/tmp/cmds.gdb") == 0);
  snprintf(expected_debugger_cmd, sizeof(expected_debugger_cmd),
           "gdb -q '/tmp/fweelin' %d 2>/dev/null <'/tmp/cmds.gdb' >fweelin-stackdump",
           (int)getpid());
  assert(strcmp(debugger_cmd, expected_debugger_cmd) == 0);
  for (i = 0; debugger_cmd[i] != '\0'; i++)
    assert(debugger_cmd[i] != '\n');

  assert(stacktrace_build_debugger_command(
             quoted_debugger_cmd, sizeof(quoted_debugger_cmd),
             "/tmp/fweelin';echo pwnd #", "/tmp/cmds';cat /etc/passwd #.gdb") ==
         0);
  snprintf(expected_debugger_cmd, sizeof(expected_debugger_cmd),
           "gdb -q '/tmp/fweelin'\"'\"';echo pwnd #' %d 2>/dev/null <'/tmp/cmds'\"'\"';cat /etc/passwd #.gdb' >fweelin-stackdump",
           (int)getpid());
  assert(strcmp(quoted_debugger_cmd, expected_debugger_cmd) == 0);

  return 0;
}
