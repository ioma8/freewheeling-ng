#include "fweelin_string_utils.h"

#include <assert.h>
#include <string.h>

int main() {
  char long_token[400];
  memset(long_token, 'x', sizeof(long_token) - 1);
  long_token[sizeof(long_token) - 1] = '\0';

  const char *cursor = long_token;
  const FweelinTokenSpan first =
      fweelin_split_token(cursor, '\0');
  assert(first.len == strlen(long_token));
  assert(first.next == 0);
  assert(strncmp(first.begin, long_token, first.len) == 0);

  char pair[900];
  memset(pair, 0, sizeof(pair));
  strcpy(pair, "12.5,");
  memset(pair + 5, '7', 600);
  pair[605] = '\0';

  cursor = pair;
  const FweelinTokenSpan left =
      fweelin_split_token(cursor, ',');
  assert(left.len == 4);
  assert(strncmp(left.begin, "12.5", left.len) == 0);
  assert(left.next != 0);

  const FweelinTokenSpan right =
      fweelin_split_token(left.next, ',');
  assert(right.len == 600);
  assert(right.next == 0);
  for (size_t i = 0; i < right.len; ++i)
    assert(right.begin[i] == '7');

  return 0;
}
