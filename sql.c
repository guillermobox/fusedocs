/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "sql.h"

static char dbname[] = "/tmp/test.db";
static char initfile[] = "create_commands.sql";
static sqlite3 *connection;
const int EPATHFORMAT = -1;
const int EPATHNOFOUND = -2;

void destroy_db(void *conn){
	sqlite3_close(connection);
};

void *init_db(struct fuse_conn_info *conninfo)
{
	sqlite3_stmt *sqlcursor;
	int ret;

	if (access(dbname, F_OK) != -1) {
		sqlite3_open_v2(dbname, &connection,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				NULL);
	} else {
		FILE *f;
		char inputline[128];

		f = fopen(initfile, "r");
		sqlite3_open_v2(dbname, &connection,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				NULL);

		while (!feof(f)) {
			if (fgets(inputline, 128, f) == NULL)
				continue;
			ret =
			    sqlite3_prepare_v2(connection, inputline, 128,
					       &sqlcursor, NULL);
			if (ret != SQLITE_OK) {
				printf("Error preparing [%s]!\n",
				       inputline);
				exit(1);
			}
			ret = sqlite3_step(sqlcursor);
			if (ret != SQLITE_DONE) {
				printf("Error stepping [%s]\n", inputline);
				exit(1);
			}
			ret = sqlite3_finalize(sqlcursor);
			if (ret != SQLITE_OK) {
				printf("Error finalizing [%s]\n",
				       inputline);
				exit(1);
			}
		}
		fclose(f);
	}
	return NULL;
};

static int find_file(const char *filename, struct stat *stbuf)
{
	sqlite3_stmt *cur;
	char statement[128];
	int err;

	sprintf(statement, "SELECT id,size FROM FileIndex WHERE name=?");

	err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_text(cur, 1, filename, strlen(filename), NULL);
	if (err != SQLITE_OK) {
		printf("Error doing [%s]!\n", statement);
		exit(1);
	}

	err = sqlite3_step(cur);
	if (err == SQLITE_DONE) {
		sqlite3_finalize(cur);
		return EPATHNOFOUND;
	} else {
		int ret = sqlite3_column_int(cur, 0);
		stbuf->st_size = sqlite3_column_int(cur,1);
		sqlite3_finalize(cur);
		return ret;
	}
}

int checkpath(const char *path, struct stat *stbuf)
{
	int id;
	if (path[0] != '/') {
		return EPATHFORMAT;
	} else {
		if (!strncmp(path, "/", strlen(path))) {
			return 0;
		} else {
			id = find_file(path + 1, stbuf);
			return id;
		}
	}
	return EPATHNOFOUND;
}

char *setpath(const char *path, int id, char *content, int size)
{
	sqlite3_stmt *cur;
	char statement[128];
	int ret;

	path = path + 1;

	printf("Setting this path: %s[%d] with this size: %d\n", path, id, size);
	sprintf(statement, "UPDATE Blobs SET content=? WHERE rowid=?");
	ret = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_blob(cur, 1, content, size, SQLITE_TRANSIENT);
	sqlite3_bind_int(cur, 2, id);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}
	ret = sqlite3_step(cur);
	if (ret != SQLITE_DONE)
		printf("ERROR, NOT DONE!\n");
	sqlite3_finalize(cur);

	sprintf(statement, "UPDATE FileIndex SET size=? WHERE name=?");
	ret = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_int(cur, 1, size);
	sqlite3_bind_text(cur, 2, path, strlen(path), NULL);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}
	ret = sqlite3_step(cur);
	if (ret != SQLITE_DONE)
		printf("ERROR, NOT DONE!\n");
	sqlite3_finalize(cur);
	return NULL;
}

int db_readfile(int blobid, struct st_file_buffer *buffer){
	sqlite3_stmt *cur;
	char inputline[128];
	int ret;

	sprintf(inputline, "SELECT content FROM Blobs WHERE rowid=%d;",
		blobid);

	ret = sqlite3_prepare_v2(connection, inputline, 128, &cur, NULL);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", inputline);
		exit(1);
	}

	ret = sqlite3_step(cur);
	buffer->data = strdup((const char *) sqlite3_column_text(cur, 0));
	buffer->allocated = sqlite3_column_bytes(cur, 0);
	buffer->used = buffer->allocated;
	printf("File size found for %d: %d\n", blobid, buffer->used);
	sqlite3_finalize(cur);
	return 0;
}

char **listpath(int *n)
{
	sqlite3_stmt *cur;
	char statement[128];
	char **arraylist;
	int err;
	int count = 0, allocated = 128;

	arraylist = malloc(sizeof(char *) * allocated);

	sprintf(statement, "SELECT name FROM FileIndex");

	err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	if (err != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}

	do {
		err = sqlite3_step(cur);
		if (err == SQLITE_DONE)
			break;
		arraylist[count] =
		    strdup((const char *) sqlite3_column_text(cur, 0));
		count += 1;
	} while (1);

	*n = count;
	return arraylist;
}

int createpath(const char *path)
{
	sqlite3_stmt *cur;
	char statement[128];
	char *content = "";
	int id, ret, size = 0;

	sprintf(statement, "INSERT INTO Blobs VALUES (?)");
	ret = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_blob(cur, 1, content, size, SQLITE_STATIC);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}

	ret = sqlite3_step(cur);

	if (ret != SQLITE_DONE) {
		printf("ERROR, NOT DONE!\n");
		sqlite3_finalize(cur);
	};
	sqlite3_finalize(cur);

	id = sqlite3_last_insert_rowid(connection);

	sprintf(statement,
		"INSERT INTO FileIndex (blobid, name) VALUES (?,?)");
	sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_int(cur, 1, id);
	sqlite3_bind_text(cur, 2, path, strlen(path), NULL);
	sqlite3_step(cur);
	sqlite3_finalize(cur);
	return 0;
}

int deletepath(const char *path)
{
	sqlite3_stmt *cur;
	char statement[128];
	int err;
	sprintf(statement, "DELETE FROM FileIndex WHERE name=?");
	err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	if (err != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}
	sqlite3_bind_text(cur, 1, path, -1, NULL);
	err = sqlite3_step(cur);
	return 0;
}

int renamepath(const char *oldpath, const char *newpath)
{
	sqlite3_stmt *cur;
	char statement[128];
	int err;
	sprintf(statement, "UPDATE FileIndex SET name=? WHERE name=?");
	err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	if (err != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}
	sqlite3_bind_text(cur, 1, newpath, -1, NULL);
	sqlite3_bind_text(cur, 2, oldpath, -1, NULL);
	err = sqlite3_step(cur);
	return 0;
}
