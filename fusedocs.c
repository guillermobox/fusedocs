/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "sql.h"

#define FUSE_USE_VERSION 26
#include <fuse.h>

static int fusedoc_mknod(const char *path, mode_t mode, dev_t dev){
	int ret;
	ret = createpath(path+1);
	if (ret) {
		return -1;
	} else {
		return 0;
	}
};

static int fusedoc_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	int id;
	char *content;
	int size;

	id = checkpath(path);
	getpath(id, &content, &size);

	memset(stbuf, 0, sizeof(struct stat));
	if (id == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (id > 0) {
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		stbuf->st_size = size - 1;
	} else
		res = -ENOENT;

	return res;
}

static int fusedoc_readdir(const char *path, void *buf,
			   fuse_fill_dir_t filler, off_t offset,
			   struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	char **paths;
	int ammount, i;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	paths = listpath(&ammount);

	for (i = 0; i < ammount; i++) {
		filler(buf, paths[i], NULL, 0);
		free(paths[i]);
	}

	free(paths);

	return 0;
}

static int fusedoc_open(const char *path, struct fuse_file_info *fi)
{
	int id;

	id = checkpath(path);

	if (id < 1)
		return -ENOENT;

	return 0;
}

static int fusedoc_write(const char *path, const char *buf, size_t size,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	char *contents, *newcontents;
	int id, length;

	id = checkpath(path);

	if (id < 0)
		return -ENOENT;

	getpath(id, &contents, &length);

	newcontents = malloc((size + offset) * sizeof(char));
	strncpy(newcontents, contents, length);
	strncpy(newcontents + offset, buf, size);

	setpath(id, newcontents, size);

	return size;
}

static int fusedoc_truncate(const char *path, off_t size)
{
	int id, length;
	char *contents, *newcontents;

	id = checkpath(path);

	if (id < 0)
		return -ENOENT;

	getpath(id, &contents, &length);

	newcontents = strndup(contents, size);
	setpath(id, newcontents, size);

	return 0;
}

static int fusedoc_read(const char *path, char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	int id, length;
	char *contents;

	id = checkpath(path);

	if (id < 0)
		return -ENOENT;

	getpath(id, &contents, &length);

	len = length - 1;
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, contents + offset, size);
	} else {
		size = 0;
	}

	return size;
}

struct fuse_operations fusedoc_operations = {
	.getattr = fusedoc_getattr,
	.readdir = fusedoc_readdir,
	.open = fusedoc_open,
	.read = fusedoc_read,
	.truncate = fusedoc_truncate,
	.write = fusedoc_write,
	.mknod = fusedoc_mknod,
};

int main(int argc, char **argv)
{
	return fuse_main(argc, argv, &fusedoc_operations, NULL);
}
