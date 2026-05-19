#ifndef __FWEELIN_STRING_UTILS_H
#define __FWEELIN_STRING_UTILS_H

#include <stddef.h>
#include <string.h>

struct FweelinTokenSpan {
  const char *begin;
  size_t len;
  const char *next;
};

enum FweelinPathExpandResult {
  FWEELIN_PATH_EXPAND_OK = 0,
  FWEELIN_PATH_EXPAND_TRUNCATED,
  FWEELIN_PATH_EXPAND_MISSING_HOME
};

inline FweelinTokenSpan fweelin_split_token(const char *src, char delim) {
  FweelinTokenSpan span;
  span.begin = (src != 0 ? src : "");
  span.len = 0;
  span.next = 0;

  if (src == 0)
    return span;

  while (src[span.len] != '\0' &&
         (delim == '\0' || src[span.len] != delim)) {
    span.len++;
  }

  if (src[span.len] == delim && delim != '\0')
    span.next = src + span.len + 1;

  return span;
}

inline char *fweelin_dup_token(const FweelinTokenSpan &span) {
  char *dst = new char[span.len + 1];
  if (span.len > 0)
    memcpy(dst, span.begin, span.len);
  dst[span.len] = '\0';
  return dst;
}

inline size_t fweelin_copy_truncate(char *dst, size_t dst_size,
                                    const char *src) {
  size_t pos = 0;

  if (dst == 0 || dst_size == 0)
    return 0;

  if (src != 0) {
    while (pos + 1 < dst_size && src[pos] != '\0') {
      dst[pos] = src[pos];
      pos++;
    }
  }

  dst[pos] = '\0';
  return pos;
}

inline size_t fweelin_append_truncate(char *dst, size_t dst_size,
                                      const char *src) {
  size_t pos = 0;

  if (dst == 0 || dst_size == 0)
    return 0;

  while (pos < dst_size && dst[pos] != '\0')
    pos++;

  if (pos == dst_size) {
    dst[dst_size - 1] = '\0';
    return dst_size - 1;
  }

  if (src != 0) {
    while (pos + 1 < dst_size && *src != '\0')
      dst[pos++] = *src++;
  }

  dst[pos] = '\0';
  return pos;
}

inline bool fweelin_copy_filename_truncate(char *dst, size_t dst_size,
                                           const char *src) {
  const size_t copied = fweelin_copy_truncate(dst, dst_size, src);

  if (src == 0)
    return false;

  return src[copied] != '\0';
}

inline FweelinPathExpandResult fweelin_expand_home_path(
    char *dst, size_t dst_size, const char *src, const char *home_dir) {
  if (dst == 0 || dst_size == 0) {
    return FWEELIN_PATH_EXPAND_TRUNCATED;
  }

  if (src == 0) {
    dst[0] = '\0';
    return FWEELIN_PATH_EXPAND_OK;
  }

  if (src[0] != '~') {
    return fweelin_copy_filename_truncate(dst, dst_size, src)
               ? FWEELIN_PATH_EXPAND_TRUNCATED
               : FWEELIN_PATH_EXPAND_OK;
  }

  if (home_dir == 0 || home_dir[0] == '\0') {
    dst[0] = '\0';
    return FWEELIN_PATH_EXPAND_MISSING_HOME;
  }

  const size_t copied = fweelin_copy_truncate(dst, dst_size, home_dir);
  const size_t expanded =
      fweelin_append_truncate(dst, dst_size, src + 1);

  if (home_dir[copied] != '\0' || src[1 + (expanded - copied)] != '\0')
    return FWEELIN_PATH_EXPAND_TRUNCATED;

  return FWEELIN_PATH_EXPAND_OK;
}

inline char *fweelin_alloc_saveable_stub(const char *basename,
                                         const char *hashtext,
                                         const char *objname,
                                         const char *ext) {
  const char *safe_basename = (basename != 0 ? basename : "");
  const char *safe_hashtext = (hashtext != 0 ? hashtext : "");
  const char *safe_objname =
      (objname != 0 && objname[0] != '\0' ? objname : 0);
  const char *safe_ext = (ext != 0 ? ext : "");

  size_t len = strlen(safe_basename) + 1 + strlen(safe_hashtext) +
               strlen(safe_ext);
  if (safe_objname != 0)
    len += 1 + strlen(safe_objname);

  char *dst = new char[len + 1];
  size_t pos = fweelin_copy_truncate(dst, len + 1, safe_basename);
  dst[pos++] = '-';
  dst[pos] = '\0';
  pos = fweelin_append_truncate(dst, len + 1, safe_hashtext);

  if (safe_objname != 0) {
    dst[pos++] = '-';
    dst[pos] = '\0';
    fweelin_append_truncate(dst, len + 1, safe_objname);
  }

  fweelin_append_truncate(dst, len + 1, safe_ext);
  return dst;
}

inline char *fweelin_alloc_saveable_path(const char *library_path,
                                         const char *basename,
                                         const char *hashtext,
                                         const char *objname,
                                         const char *ext) {
  const char *safe_library_path = (library_path != 0 ? library_path : "");
  char *stub =
      fweelin_alloc_saveable_stub(basename, hashtext, objname, ext);
  const size_t library_len = strlen(safe_library_path);
  const size_t stub_len = strlen(stub);
  char *dst = new char[library_len + 1 + stub_len + 1];
  size_t pos = fweelin_copy_truncate(dst, library_len + 1 + stub_len + 1,
                                     safe_library_path);
  dst[pos++] = '/';
  dst[pos] = '\0';
  fweelin_append_truncate(dst, library_len + 1 + stub_len + 1, stub);
  delete[] stub;
  return dst;
}

#endif
