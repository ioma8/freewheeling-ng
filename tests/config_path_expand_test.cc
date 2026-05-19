#include "fweelin_string_utils.h"

#include <assert.h>
#include <string.h>

int main() {
  char buf[128];

  assert(fweelin_expand_home_path(buf, sizeof(buf), "/tmp/freewheeling",
                                  "/Users/test") ==
         FWEELIN_PATH_EXPAND_OK);
  assert(strcmp(buf, "/tmp/freewheeling") == 0);

  assert(fweelin_expand_home_path(buf, sizeof(buf), "~/Library/Application Support/fweelin",
                                  "/Users/test") ==
         FWEELIN_PATH_EXPAND_OK);
  assert(strcmp(buf, "/Users/test/Library/Application Support/fweelin") == 0);

  assert(fweelin_expand_home_path(buf, sizeof(buf), "~/library", 0) ==
         FWEELIN_PATH_EXPAND_MISSING_HOME);
  assert(buf[0] == '\0');

  char long_home[96];
  memset(long_home, 'a', sizeof(long_home) - 1);
  long_home[0] = '/';
  long_home[sizeof(long_home) - 1] = '\0';
  assert(fweelin_expand_home_path(buf, 32, "~/library", long_home) ==
         FWEELIN_PATH_EXPAND_TRUNCATED);
  assert(buf[31] == '\0');

  return 0;
}
