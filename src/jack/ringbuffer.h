#ifndef JACK_RINGBUFFER_H
#define JACK_RINGBUFFER_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct jack_ringbuffer {
  char *buf;
  size_t size;
  volatile size_t write_pos;
  volatile size_t read_pos;
} jack_ringbuffer_t;

typedef struct jack_ringbuffer_data {
  char *buf;
  size_t len;
} jack_ringbuffer_data_t;

static inline jack_ringbuffer_t *jack_ringbuffer_create(size_t sz) {
  jack_ringbuffer_t *rb = (jack_ringbuffer_t *)calloc(1, sizeof(jack_ringbuffer_t));
  if (rb == NULL)
    return NULL;
  rb->buf = (char *)calloc(1, sz);
  if (rb->buf == NULL) {
    free(rb);
    return NULL;
  }
  rb->size = sz;
  return rb;
}

static inline void jack_ringbuffer_free(jack_ringbuffer_t *rb) {
  if (rb == NULL)
    return;
  free(rb->buf);
  free(rb);
}

static inline size_t jack_ringbuffer_read_space(const jack_ringbuffer_t *rb) {
  size_t w = rb->write_pos;
  size_t r = rb->read_pos;
  if (w >= r)
    return w - r;
  return rb->size - (r - w);
}

static inline size_t jack_ringbuffer_write_space(const jack_ringbuffer_t *rb) {
  return rb->size - jack_ringbuffer_read_space(rb) - 1;
}

static inline void jack_ringbuffer_reset(jack_ringbuffer_t *rb) {
  rb->write_pos = 0;
  rb->read_pos = 0;
}

static inline size_t jack_ringbuffer_write(jack_ringbuffer_t *rb, const char *src, size_t cnt) {
  size_t space = jack_ringbuffer_write_space(rb);
  if (cnt > space)
    cnt = space;
  if (cnt == 0)
    return 0;

  size_t first = rb->size - rb->write_pos;
  if (first > cnt)
    first = cnt;
  memcpy(rb->buf + rb->write_pos, src, first);
  memcpy(rb->buf, src + first, cnt - first);
  rb->write_pos = (rb->write_pos + cnt) % rb->size;
  return cnt;
}

static inline size_t jack_ringbuffer_read(jack_ringbuffer_t *rb, char *dst, size_t cnt) {
  size_t avail = jack_ringbuffer_read_space(rb);
  if (cnt > avail)
    cnt = avail;
  if (cnt == 0)
    return 0;

  size_t first = rb->size - rb->read_pos;
  if (first > cnt)
    first = cnt;
  memcpy(dst, rb->buf + rb->read_pos, first);
  memcpy(dst + first, rb->buf, cnt - first);
  rb->read_pos = (rb->read_pos + cnt) % rb->size;
  return cnt;
}

#endif
