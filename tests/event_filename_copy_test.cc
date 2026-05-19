#include "fweelin_string_utils.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

int main() {
  char exact[PATH_MAX];
  memset(exact, 'a', sizeof(exact) - 1);
  exact[sizeof(exact) - 1] = '\0';

  char buf[PATH_MAX];
  const bool truncated_exact =
      fweelin_copy_filename_truncate(buf, sizeof(buf), exact);
  assert(truncated_exact == false);
  assert(strcmp(buf, exact) == 0);

  char too_long[PATH_MAX + 32];
  memset(too_long, 'b', sizeof(too_long) - 1);
  too_long[sizeof(too_long) - 1] = '\0';

  const bool truncated_long =
      fweelin_copy_filename_truncate(buf, sizeof(buf), too_long);
  assert(truncated_long == true);
  assert(buf[sizeof(buf) - 1] == '\0');
  assert(strlen(buf) == sizeof(buf) - 1);

  const bool truncated_null =
      fweelin_copy_filename_truncate(buf, sizeof(buf), 0);
  assert(truncated_null == false);
  assert(buf[0] == '\0');

  return 0;
}
