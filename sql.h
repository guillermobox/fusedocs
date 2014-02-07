#include <sqlite3.h>
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "buffer.h"
#include "fusedocs.h"

#ifndef _SQL_H_
#define _SQL_H_

void * init_db(struct fuse_conn_info *conninfo);
void destroy_db(void *conn);

int db_readfile(int, struct st_file_buffer *);
int db_newtag(const char *);
char ** db_listtags(int *n, struct st_path *);

char *getpath(int id, char **content, int *length);
char *setpath(const char *,int id, char *content, int size);
int createpath(struct st_path *);
int checkpath(struct st_path *, struct stat *stbuf);
char **listpath(int *n, struct st_path *);
int deletepath(const char *path);
int renamepath(struct st_path *, struct st_path *);

const int EPATHFORMAT;
const int EPATHNOFOUND;

#endif
