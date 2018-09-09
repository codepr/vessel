#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdio.h>
#include <stdint.h>


typedef struct ringbuf Ringbuf;

/* Initialize the structure by associating a byte buffer, alloc on the heap so
   it has to be freed with ringbuf_free */
Ringbuf *ringbuf_init(uint8_t *, size_t);

/* Free the circular buffer */
void ringbuf_free(Ringbuf *);

/* Make tail = head and full to false (an empty ringbuf) */
void ringbuf_reset(Ringbuf *);

/* Push a single byte into the buffer and move forward the interator pointer */
int8_t ringbuf_push(Ringbuf *, uint8_t);

/* Push each element of a bytearray into the buffer */
int8_t ringbuf_bulk_push(Ringbuf *, uint8_t *, size_t);

/* Pop out the front of the buffer */
int8_t ringbuf_pop(Ringbuf *, uint8_t *);

/* Pop out a number of bytes from the ringbuffer defined by a len variable */
int8_t ringbuf_bulk_pop(Ringbuf *, uint8_t *, size_t);

/* Check if the buffer is empty, returning 0 or 1 according to the result */
uint8_t ringbuf_empty(Ringbuf *);

/* Check if the buffer is full, returning 0 or 1 according to the result */
uint8_t ringbuf_full(Ringbuf *);

/* Return the max size of the buffer, e.g. the bytearray size used to init the
   buffer */
size_t ringbuf_capacity(Ringbuf *);

/* Return the current size of the buffer, e.g. the nr of bytes currently stored
   inside */
size_t ringbuf_size(Ringbuf *);


#endif
