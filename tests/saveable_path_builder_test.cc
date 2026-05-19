#include "fweelin_string_utils.h"

#include <assert.h>
#include <string.h>

#include <string>

int main() {
  const char *hash = "0123456789abcdef0123456789abcdef";
  std::string library_path(5000, 'L');
  std::string object_name(6000, 'N');

  char *full_path = fweelin_alloc_saveable_path(library_path.c_str(),
                                                "loop",
                                                hash,
                                                object_name.c_str(),
                                                ".wav");
  assert(full_path != 0);

  std::string expected_full_path =
      library_path + "/loop-" + hash + "-" + object_name + ".wav";
  assert(strcmp(full_path, expected_full_path.c_str()) == 0);
  delete[] full_path;

  char *stub_without_name =
      fweelin_alloc_saveable_stub("scene", hash, "", 0);
  assert(stub_without_name != 0);
  assert(strcmp(stub_without_name,
                "scene-0123456789abcdef0123456789abcdef") == 0);
  delete[] stub_without_name;

  char *stub_with_name =
      fweelin_alloc_saveable_stub("scene", hash, "chorus", ".xml");
  assert(stub_with_name != 0);
  assert(strcmp(stub_with_name,
                "scene-0123456789abcdef0123456789abcdef-chorus.xml") == 0);
  delete[] stub_with_name;

  return 0;
}
