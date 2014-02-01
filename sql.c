/*
 * Copyright 2013 Guillermo Indalecio Fernandez <guillermobox@gmail.com>
 */
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "sql.h"

static char dbfile[] = "/tmp/test.db";
static char initfile[] = "create_commands.sql";
const int EPATHFORMAT = -1;
const int EPATHNOFOUND = -2;

static sqlite3 *test_and_create(char *dbname)
{
	sqlite3 *ptr;
	sqlite3_stmt *sqlcursor;
	FILE *f;
	char inputline[128];
	int ret;

	if (access(dbname, F_OK) != -1) {
		sqlite3_open_v2(dbname, &ptr,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				NULL);
	} else {

		f = fopen(initfile, "r");
		sqlite3_open_v2(dbname, &ptr,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				NULL);

		while (!feof(f)) {
			if (fgets(inputline, 128, f) == NULL)
				continue;
			ret =
			    sqlite3_prepare_v2(ptr, inputline, 128,
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

	return ptr;
}

static int find_file(const char *filename, sqlite3 * con)
{
	sqlite3_stmt *cur;
	char statement[128];
	int err;

	sprintf(statement, "SELECT id FROM FileIndex WHERE name=\"%s\"",
		filename);

	err = sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
	if (err != SQLITE_OK) {
		printf("Error doing [%s]!\n", statement);
		exit(1);
	}

	err = sqlite3_step(cur);
	if (err == SQLITE_DONE) {
		sqlite3_finalize(cur);
		sqlite3_close(con);
		return EPATHNOFOUND;
	} else {
		int ret = sqlite3_column_int(cur, 0);
		sqlite3_finalize(cur);
		sqlite3_close(con);
		return ret;
	}
}

int checkpath(const char *path)
{
	int id;
	if (path[0] != '/') {
		return EPATHFORMAT;
	} else {
		sqlite3 *con;
		con = test_and_create(dbfile);
		if (!strncmp(path, "/", strlen(path))) {
			sqlite3_close(con);
			return 0;
		} else {
			id = find_file(path + 1, con);
			if (id < 0) {
				printf("File not found!\n");
			} else {
				printf("File found: %d\n", id);
			}
			sqlite3_close(con);
			return id;
		}
	}
	return EPATHNOFOUND;
}

char *setpath(int id, char *content, int size)
{
	sqlite3 *con;
	sqlite3_stmt *cur;
	char statement[128];
	int ret;
	con = test_and_create(dbfile);
	sprintf(statement, "UPDATE Blobs SET content=? WHERE rowid=?");
	ret = sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
	sqlite3_bind_blob(cur, 1, content, size, SQLITE_STATIC);
	sqlite3_bind_int(cur, 2, id);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}

	ret = sqlite3_step(cur);

	if (ret != SQLITE_DONE)
		printf("ERROR, NOT DONE!\n");

	sqlite3_finalize(cur);
	sqlite3_close(con);

	return NULL;
}

char *getpath(int id, char **content, int *length)
{
	sqlite3 *con;
	sqlite3_stmt *cur;
	char inputline[128];
	int ret;

	con = test_and_create(dbfile);

	sprintf(inputline, "SELECT content FROM Blobs WHERE rowid=%d;",
		id);

	ret = sqlite3_prepare_v2(con, inputline, 128, &cur, NULL);
	if (ret != SQLITE_OK) {
		printf("Error preparing [%s]\n", inputline);
		exit(1);
	}

	ret = sqlite3_step(cur);
	if (ret == SQLITE_DONE) {
	} else {
		*content =
		    strdup((const char *) sqlite3_column_text(cur, 0));
		*length = sqlite3_column_bytes(cur, 0) + 1;
	}
	sqlite3_finalize(cur);

	sqlite3_close(con);
	return NULL;
}

char **listpath(int *n)
{
	sqlite3 *con;
	sqlite3_stmt *cur;
	char statement[128];
	char **arraylist;
	int err;
	int count = 0, allocated = 128;

	arraylist = malloc(sizeof(char *) * allocated);

	con = test_and_create(dbfile);

	sprintf(statement, "SELECT name FROM FileIndex");

	err = sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
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
	sqlite3 *con;
	sqlite3_stmt *cur;
	char statement[128];
	char *content = "";
	int id, ret, size = 0;

	con = test_and_create(dbfile);
	sprintf(statement, "INSERT INTO Blobs VALUES (?)");
	ret = sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
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

	id = sqlite3_last_insert_rowid(con);

	sprintf(statement,
		"INSERT INTO FileIndex (blobid, name) VALUES (?,?)");
	sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
	sqlite3_bind_int(cur, 1, id);
	sqlite3_bind_text(cur, 2, path, strlen(path), NULL);
	sqlite3_step(cur);
	sqlite3_finalize(cur);

	sqlite3_close(con);

	return 0;
}

int deletepath(const char *path)
{
	sqlite3 *con;
	sqlite3_stmt *cur;
	char statement[128];
	int err;
	con = test_and_create(dbfile);
	sprintf(statement, "DELETE FROM FileIndex WHERE name=?");
	err = sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
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
	sqlite3 *con;
	sqlite3_stmt *cur;
	char statement[128];
	int err;
	con = test_and_create(dbfile);
	sprintf(statement, "UPDATE FileIndex SET name=? WHERE name=?");
	err = sqlite3_prepare_v2(con, statement, 128, &cur, NULL);
	if (err != SQLITE_OK) {
		printf("Error preparing [%s]\n", statement);
		exit(1);
	}
	sqlite3_bind_text(cur, 1, newpath, -1, NULL);
	sqlite3_bind_text(cur, 2, oldpath, -1, NULL);
	err = sqlite3_step(cur);
	return 0;
}
