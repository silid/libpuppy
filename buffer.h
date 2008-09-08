#ifndef PUPPYBUFFER_H
#define PUPPYBUFFER_H

struct buffer;
typedef struct buffer *buffer_t;

buffer_t buffer_init(int a_els, int a_el_size);

// releases buffer
void buffer_done(buffer_t a_buffer);

void *buffer_begin(buffer_t a_buffer);
void *buffer_end(buffer_t a_buffer);

// returns pointer to the added value. if a_value is null, nothing is
// copied, space is allocated.
void *buffer_add(buffer_t a_buffer, void *a_value);

void *buffer_dup_values(buffer_t a_buffer);

#endif // PUPPYBUFFER_H
