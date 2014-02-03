#include <sqlite3.h>
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "buffer.h"

#ifndef _SQL_H_
#define _SQL_H_

void * init_db(struct fuse_conn_info *conninfo);
void destroy_db(void *conn);

int db_readfile(int, struct st_file_buffer *);
int db_newtag(const char *);

char *getpath(int id, char **content, int *length);
char *setpath(const char *,int id, char *content, int size);
int createpath(const char *path);
int checkpath(const char *path, struct stat *stbuf);
char **listpath(int *n);
int deletepath(const char *path);
int renamepath(const char *oldpath, const char *newpath);

const int EPATHFORMAT;
const int EPATHNOFOUND;

#endif
