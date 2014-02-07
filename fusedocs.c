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
#include "fusedocs.h"

#define FUSE_USE_VERSION 26
#include <fuse.h>

void create_path(const char *path, struct st_path *stpath){
	char * ptr, *newptr;
	char slash = '/';

	ptr = (char*)path;
	stpath->basename = NULL;
	stpath->ntokens = 0;

	while( (newptr = strchr(ptr, slash)) != NULL){
		ptr = newptr+1;
		newptr = strchr(ptr, slash);
		if (newptr!=NULL){
			*newptr = '\0';
			stpath->tokens[stpath->ntokens++] = strdup(ptr);
			*newptr = '/';
		}
		continue;
	}

	stpath->basename = strdup(ptr);
};

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

	struct st_path stpath;
	create_path(path, &stpath);

	ret = createpath(&stpath);
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
	struct st_path stpath;
	create_path(path, &stpath);

	memset(stbuf, 0, sizeof(struct stat));
	id = checkpath(stpath.basename, stbuf);

	if (id == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (id > 0) {
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
	} else
		res = -ENOENT;

	return res;
}

static int fusedoc_readdir(const char *path, void *buf,
			   fuse_fill_dir_t filler, off_t offset,
			   struct fuse_file_info *fi)
{
	char **paths;
	int ammount, i;
	struct st_path stpath;
	char * newpath;

	if (strcmp(path,"/") !=0 ) {
		newpath = malloc(strlen(path)+2);
		strcpy(newpath, path);
		strcat(newpath, "/");
	} else {
		newpath =(char*) path;
	}

	create_path(newpath, &stpath);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	paths = listpath(&ammount, &stpath);
	for (i = 0; i < ammount; i++) {
		filler(buf, paths[i], NULL, 0);
		free(paths[i]);
	}
	free(paths);

	paths = db_listtags(&ammount, &stpath);
	for (i = 0; i < ammount; i++) {
		filler(buf, paths[i], NULL, 0);
		free(paths[i]);
	}
	free(paths);

	return 0;
}

/*
 * fusedoc_mkdir: create a directory with the given name and permissions
 */
static int fusedoc_mkdir(const char *path, mode_t dirmode)
{
	dirmode = dirmode | S_IFDIR;
	return db_newtag(path+1);
}

/*
 * fusedoc_open: create a buffer with the file contents, keep it until
 * the release signal.
 */
static int fusedoc_open(const char *path, struct fuse_file_info *fi)
{
	int id, index;
	struct stat filest;
	struct st_path stpath;

	create_path(path, &stpath);
	id = checkpath(stpath.basename, &filest);

	if (id < 1)
		return -ENOENT;

	for (index = 0; index < BUFFERLEN; index++) {
		if (buffer_array[index].data == NULL)
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
	struct st_file_buffer * buffer;
	struct stat filest;
	int id;

	struct st_path stpath;
	create_path(path, &stpath);

	buffer = buffer_array + fi->fh;
	id = checkpath(stpath.basename, &filest);

	setpath(stpath.basename, id, buffer->data, buffer->used);

	return 0;
}

/*
 * fusedoc_write: write in the buffer asociated with the given
 * file
 */
static int fusedoc_write(const char *path, const char *buf, size_t size,
			 off_t offset, struct fuse_file_info *fi)
{
	struct st_file_buffer *buffer;

	/*
		int id;
		struct stat filest;
		struct st_path stpath;
		create_path(stpath.basename, &stpath);
		id = checkpath(stpath.basename, &filest);
		if (id < 0)
			return -ENOENT;
	*/

	buffer = buffer_array + fi->fh;
	return buffer_write(buffer, size, offset, buf);
}

/*
 * fusedoc_ftruncate: truncate the size from the buffer, from open file
 */
static int fusedoc_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	int id;
	struct stat filest;
	struct st_file_buffer * buffer;
	struct st_path stpath;
	create_path(path, &stpath);

	id = checkpath(stpath.basename, &filest);

	if (id < 0)
		return -ENOENT;

	buffer = buffer_array + fi->fh;
	buffer_truncate(buffer, size);

	return 0;
}

/*
 * fusedoc_read: read from the buffer asociated with the given
 * file
 */
static int fusedoc_read(const char *path, char *dest, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	struct st_file_buffer * buffer;
	buffer = buffer_array + fi->fh;
	return (int) buffer_read(buffer, size, offset, dest);
}

/*
 * fusedoc_init: initiate the filesystem, open access to sql database
 */
void *fusedoc_init(struct fuse_conn_info *conninfo){
	int i;
	for (i=0; i<BUFFERLEN; i++) {
		buffer_array[i].data = NULL;
	}
	return init_db(conninfo);
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
	destroy_db(conn);
};

struct fuse_operations fusedoc_operations = {
	.getattr = fusedoc_getattr,
	.readdir = fusedoc_readdir,
	.open = fusedoc_open,
	.release = fusedoc_release,
	.read = fusedoc_read,
	.ftruncate = fusedoc_ftruncate,
	.truncate = fusedoc_ftruncate,
	.write = fusedoc_write,
	.mknod = fusedoc_mknod,
	.mkdir = fusedoc_mkdir,
	.unlink = fusedoc_unlink,
	.rename = fusedoc_rename,
	.destroy = fusedoc_destroy,
	.init = fusedoc_init,
};

int main(int argc, char **argv)
{
	return fuse_main(argc, argv, &fusedoc_operations, NULL);
}
