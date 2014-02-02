/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "sql.h"
#include "buffer.h"

#define FUSE_USE_VERSION 26
#include <fuse.h>

#define BUFFERLEN 128

struct st_file_buffer buffer_array[BUFFERLEN];

static int fusedoc_rename(const char *oldpath, const char *newpath){
	int ret;
	ret = renamepath(oldpath+1, newpath+1);
	if (ret) {
		return -1;
	} else {
		return 0;
	}
};

static int fusedoc_mknod(const char *path, mode_t mode, dev_t dev){
	int ret;
	ret = createpath(path+1);
	if (ret) {
		return -1;
	} else {
		return 0;
	}
};

static int fusedoc_unlink(const char *path){
	int ret;
	ret = deletepath(path+1);
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
	size = 128;
	//getpath(id, &content, &size);

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

/*
 * fusedoc_open: create a buffer with the file contents, keep it until
 * the release signal.
 */
static int fusedoc_open(const char *path, struct fuse_file_info *fi)
{
	int id, index;

	id = checkpath(path);

	if (id < 1)
		return -ENOENT;

	for (index = 0; index < BUFFERLEN; index++) {
		if (buffer_array[index].data != NULL)
			break;
	}

	db_readfile(id, buffer_array + index);
	fi->fh = index;

	return 0;
}

/*
 * fusedoc_release: delete the buffer with the file contents,
 * destroy everything allocated for that particular file
 */
static int fusedoc_release(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

/*
 * fusedoc_write: write in the buffer asociated with the given
 * file
 */
static int fusedoc_write(const char *path, const char *buf, size_t size,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	char *contents, *newcontents;
	int id, length;

	id = checkpath(path);

	if (id < 0)
		return -ENOENT;

	//getpath(id, &contents, &length);

	newcontents = malloc((size + offset) * sizeof(char));
	strncpy(newcontents, contents, length);
	strncpy(newcontents + offset, buf, size);

	setpath(id, newcontents, size);

	return size;
}

/*
 * fusedoc_truncate: truncate the size from the buffer
 */
static int fusedoc_truncate(const char *path, off_t size)
{
	int id, length;
	char *contents, *newcontents;

	id = checkpath(path);

	if (id < 0)
		return -ENOENT;

	//getpath(id, &contents, &length);

	newcontents = strndup(contents, size);
	setpath(id, newcontents, size);

	return 0;
}

/*
 * fusedoc_read: read from the buffer asociated with the given
 * file
 */
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

	//getpath(id, &contents, &length);

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

/*
 * fusedoc_init: initiate the filesystem, open access to sql database
 */
void *fusedoc_init(struct fuse_conn_info *conninfo){
	memset(buffer_array, 0, BUFFERLEN * sizeof(struct st_file_buffer));
	return init_sqlite3_database(conninfo);
};

/*
 * fusedoc_destroy: destroy the filesystem in a clean way (if possible),
 * closing access to sql database
 */
void fusedoc_destroy(void *conn){
	int i;

	for (i=0; i<BUFFERLEN; i++) {
		if (buffer_array[i].data != NULL) {
			buffer_free(buffer_array + i);
		}
	}
	destroy_sqlite3_database(conn);
};

struct fuse_operations fusedoc_operations = {
	.getattr = fusedoc_getattr,
	.readdir = fusedoc_readdir,
	.open = fusedoc_open,
//	.read = fusedoc_read,
//	.truncate = fusedoc_truncate,
//	.write = fusedoc_write,
	.mknod = fusedoc_mknod,
	.unlink = fusedoc_unlink,
	.rename = fusedoc_rename,
	.destroy = fusedoc_destroy,
	.init = fusedoc_init,
};

int main(int argc, char **argv)
{
	return fuse_main(argc, argv, &fusedoc_operations, NULL);
}
