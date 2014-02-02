/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */
#include <string.h>
#include "buffer.h"

size_t buffer_read(struct st_file_buffer *buf, size_t length, off_t offset, char *dest){
	return memcpy(dest, buf->data + offset, length);
}

size_t buffer_write(struct st_file_buffer *, size_t, off_t, char *orig);

void buffer_free(struct st_file_buffer *buf){
	if (buf->data)
		free(buf->data);
};
