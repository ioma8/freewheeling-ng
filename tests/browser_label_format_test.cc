#include "fweelin_string_utils.h"

#include <assert.h>
#include <string.h>

int main() {
  char buf[256];
  const char *prefix = "loop-";
  const char *long_name =
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";

  fweelin_copy_truncate(buf, sizeof(buf), prefix);
  fweelin_append_truncate(buf, sizeof(buf), long_name);

  assert(buf[sizeof(buf) - 1] == '\0');
  assert(strncmp(buf, prefix, strlen(prefix)) == 0);
  assert(strlen(buf) == sizeof(buf) - 1);

  return 0;
}
