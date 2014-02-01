#include <sqlite3.h>

char *getpath(int id, char **content, int *length);
char *setpath(int id, char *content, int size);
int createpath(const char *path);
int checkpath(const char *path);
char **listpath(int *n);
int deletepath(const char *path);
int renamepath(const char *oldpath, const char *newpath);

const int EPATHFORMAT;
const int EPATHNOFOUND;
