#include <sqlite3.h>

char *getpath(int id, char **content, int *length);
char *setpath(int id, char *content, int size);
int createpath(char *path);
int checkpath(const char *path);
char **listpath(int *n);

const int EPATHFORMAT;
const int EPATHNOFOUND;
