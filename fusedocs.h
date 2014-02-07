#ifndef _FUSEDOCS_H_
#define _FUSEDOCS_H_

#define BUFFERLEN 128
#define MAXSUBDIR 16

struct st_file_buffer buffer_array[BUFFERLEN];
struct st_path {
	char * tokens[MAXSUBDIR];
	char * basename;
	int ntokens;
};

#endif
