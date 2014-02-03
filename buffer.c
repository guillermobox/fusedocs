/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */
#include <string.h>
#include "buffer.h"

size_t buffer_read(struct st_file_buffer *buf, size_t length, off_t offset, char *dest)
{
	if (length+offset > buf->used) {
		memcpy(dest, buf->data + offset, buf->used - offset);
		return buf->used - offset;
	} else {
		memcpy(dest, buf->data + offset, length);
		return length;
	}
}

size_t buffer_write(struct st_file_buffer *buf, size_t length, off_t offset, const char *orig)
{
	size_t max;
	int realloc_needed = 0;

	max = length + offset;
	if (buf->allocated == 0) {
		buf->data = malloc(max);
		buf->allocated = max;
		buf->used = 0;
	}
	
	while (max > buf->allocated) {
		buf->allocated *= 2;
		realloc_needed = 1;
	}

	if (realloc_needed)
		buf->data = realloc(buf->data, buf->allocated);

	memcpy(buf->data + offset, orig, length);
	buf->used += length;
	return length;
}

void buffer_free(struct st_file_buffer *buf)
{
	if (buf->data)
		free(buf->data);
};

void buffer_truncate(struct st_file_buffer *buf, size_t length)
{
	if (length < buf->used) {
		buf->used = length;
	} else {
		buf->allocated = length;
		buf->used = length;
		buf->data = realloc(buf->data, buf->allocated);
	}
};
