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

static int find_tag(const char *tagname, struct stat *stbuf)
{
	sqlite3_stmt *cur;
	char statement[128];
	int err;

	sprintf(statement, "SELECT id FROM Tags WHERE name=?");

	err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_text(cur, 1, tagname, strlen(tagname), NULL);
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
		sqlite3_finalize(cur);
		return ret;
	}
}

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

int db_newtag(const char *tagname)
{
	sqlite3_stmt *cur;
	char statement[128];
	int ret;

	if (strncmp(tagname, ".Trash", 6)==0)
		return -1;

	sprintf(statement, "INSERT INTO Tags (name) VALUES (?)");
	ret = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_text(cur, 1, tagname, strlen(tagname), NULL);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}
	ret = sqlite3_step(cur);
	if (ret != SQLITE_DONE)
		printf("ERROR, NOT DONE!\n");
	sqlite3_finalize(cur);
	return 0;
}

int checkpath(const char *path, struct stat *stbuf)
{
	int id;
	if (strlen(path) == 0) {
		return 0;
	} else if (find_tag(path, stbuf)>=0) {
		return 0;
	} else {
		id = find_file(path, stbuf);
		return id;
	}
	return EPATHNOFOUND;
}

char *setpath(const char *path, int id, char *content, int size)
{
	sqlite3_stmt *cur;
	char statement[128];
	int ret;

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
	buffer->allocated = sqlite3_column_bytes(cur, 0);
	buffer->data = malloc(buffer->allocated);
	memcpy(buffer->data, sqlite3_column_blob(cur,0), buffer->allocated);
	buffer->used = buffer->allocated;
	sqlite3_finalize(cur);
	return 0;
}

char ** db_listtags(int *n, struct st_path *stpath)
{
	sqlite3_stmt *cur;
	char statement[128];
	char **arraylist;
	int err;
	int count = 0, allocated = 128;

	arraylist = malloc(sizeof(char *) * allocated);

	if (stpath->ntokens) {
		char * str = malloc(1024);
		int tok;

		strcpy(str, "SELECT name FROM Tags WHERE name NOT IN (");
		for (tok = 0; tok < stpath->ntokens-1; tok++) {
			strcat(str, "\"");
			strcat(str, stpath->tokens[tok]);
			strcat(str, "\", ");
		}
		strcat(str, "\"");
		strcat(str, stpath->tokens[tok]);
		strcat(str, "\"");
		strcat(str, ")");
		puts(str);
		err = sqlite3_prepare_v2(connection, str, 1024, &cur, NULL);
	} else {
		sprintf(statement, "SELECT name FROM Tags");
		err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	}

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
};

char **listpath(int *n, struct st_path *stpath)
{
	sqlite3_stmt *cur;
	char statement[128];
	char **arraylist;
	int err;
	int count = 0, allocated = 1024;

	arraylist = malloc(sizeof(char *) * allocated);

	if (stpath->ntokens) {
		char * str = malloc(1024);
		int tok;

		strcpy(str, "SELECT FileIndex.name FROM FileIndex WHERE FileIndex.id IN (");

		//SELECT fid from filetagsref where tid=1 intersect SELECT fid from filetagsref where tid=2;

		for (tok = 0; tok < stpath->ntokens; tok++) {
			strcat(str, "SELECT fid FROM FileTagsRef, Tags WHERE tid=Tags.id AND Tags.name=\"");
			strcat(str, stpath->tokens[tok]);
			strcat(str, "\" ");
			if (tok !=stpath->ntokens-1)
			strcat(str, " INTERSECT ");
		}
		strcat(str, ")");
		puts(str);

		err = sqlite3_prepare_v2(connection, str, 1024, &cur, NULL);
	} else {
		sprintf(statement, "SELECT name FROM FileIndex");
		err = sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	}

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

int createpath(struct st_path *stpath)
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

	sprintf(statement, "INSERT INTO FileIndex (blobid, name) VALUES (?,?)");
	sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
	sqlite3_bind_int(cur, 1, id);
	sqlite3_bind_text(cur, 2, stpath->basename, strlen(stpath->basename), NULL);
	sqlite3_step(cur);

	if (stpath->ntokens) {
		int tok;
		for (tok = 0; tok < stpath->ntokens; tok++) {
			sprintf(statement, " INSERT INTO FileTagsRef VALUES ( (SELECT id FROM FileIndex WHERE name=?), (SELECT id FROM Tags WHERE name=?))");
			sqlite3_prepare_v2(connection, statement, 128, &cur, NULL);
			sqlite3_bind_text(cur, 1, stpath->basename, strlen(stpath->basename), NULL);
			sqlite3_bind_text(cur, 2, stpath->tokens[tok], strlen(stpath->tokens[tok]), NULL);
			sqlite3_step(cur);
		}
	};
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
