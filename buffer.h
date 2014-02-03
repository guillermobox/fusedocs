/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdlib.h>

struct st_file_buffer {
	size_t allocated;
	size_t used;
	char *data;
};

size_t buffer_read(struct st_file_buffer *, size_t, off_t, char *);
size_t buffer_write(struct st_file_buffer *, size_t, off_t, const char *);
void buffer_free(struct st_file_buffer *);
void buffer_truncate(struct st_file_buffer *, size_t);

#endif
