#include <string.h>
#include <stdlib.h>
#include "buffer.h"

struct buffer {
  int el_size;                  /* element size */
  int cur_els;                  /* number of elements */
  int allocated_els;            /* allocated for how many elements? */
  void* data;
};

buffer_t buffer_init(int a_els, int a_el_size)
{
  buffer_t b = malloc(sizeof(struct buffer));
  if (b) {
    b->el_size = a_el_size;
    b->cur_els = 0;
    if (a_els < 1) {
      a_els = 1;
    }
    b->allocated_els = a_els;
    b->data = malloc(b->allocated_els * b->el_size);
    if (!b->data) {
      free(b); b = NULL;
    }
  }
  return b;
}

// releases buffer
void buffer_done(buffer_t a_buffer)
{
  free(a_buffer->data);
  a_buffer->data = NULL;
  free(a_buffer);
}

void* buffer_begin(buffer_t a_buffer)
{
  return a_buffer->data;
}

void* buffer_end(buffer_t a_buffer)
{
  return (char*) a_buffer->data + a_buffer->el_size * a_buffer->cur_els;
}

void* buffer_add(buffer_t a_buffer, void* a_value)
{
  void* ptr;
  while (a_buffer->cur_els >= a_buffer->allocated_els) {
    a_buffer->allocated_els *= 2;
    // doesn't handle oom
    a_buffer->data = realloc(a_buffer->data, a_buffer->el_size * a_buffer->allocated_els);
  }
  ptr = (char*) a_buffer->data + a_buffer->cur_els * a_buffer->el_size;
  if (a_value) {
    memcpy(ptr, a_value, a_buffer->el_size);
  }
  ++a_buffer->cur_els;
  return ptr;
}

void* buffer_dup_values(buffer_t a_buffer)
{
  void* ptr = malloc(a_buffer->cur_els * a_buffer->el_size);
  if (ptr) {
    memcpy(ptr, a_buffer->data, a_buffer->cur_els * a_buffer->el_size);
  }
  return ptr;
}

