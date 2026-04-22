#include <stdio.h>
#include "simfstypes.h"

/* File system operations */
void printfs(char *);
void initfs(char *);

/* Internal functions */
FILE *openfs(char *filename, char *mode);
void closefs(FILE *fp);


// * The following functions are for the createfile, readfile, writefile, deletefile,
// * and info commands. You will need to implement these functions in simfs.c.
void createfile(char *fsname, char *filename);
void deletefile(char *fsname, char *filename);
void readfile(char *fsname, char *filename, int start, int length);
void writefile(char *fsname, char *filename, int start, int length);