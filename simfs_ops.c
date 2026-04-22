/* This file contains functions that are not part of the visible interface.
 * So they are essentially helper functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simfs.h"

/* Internal helper functions first.
 */

FILE *
openfs(char *filename, char *mode)
{
    FILE *fp;
    if((fp = fopen(filename, mode)) == NULL) {
        perror("openfs");
        exit(1);
    }
    return fp;
}

void
closefs(FILE *fp)
{
    if(fclose(fp) != 0) {
        perror("closefs");
        exit(1);
    }
}

/* File system operations: creating, deleting, reading from, and writing to files.
 */

// Signatures omitted; design as you wish.
void createfile(char *fsname, char *filename)
{
    FILE *fp = openfs(fsname, "r+b");

    fentry entries[MAXFILES];

    // -----------------------------
    // 1. READ fentry table
    // -----------------------------
    rewind(fp);
    fread(entries, sizeof(fentry), MAXFILES, fp);

    // -----------------------------
    // 2. FIND EMPTY SLOT
    // -----------------------------
    int idx = -1;
    for (int i = 0; i < MAXFILES; i++) {
        if (entries[i].name[0] == '\0') {
            idx = i;
            break;
        }
    }

    // -----------------------------
    // 3. ERROR IF NO SPACE
    // -----------------------------
    if (idx == -1) {
        fprintf(stderr, "Error: no free file entries\n");
        closefs(fp);
        return;
    }

    // -----------------------------
    // 4. INSERT FILE METADATA
    // -----------------------------
    strncpy(entries[idx].name, filename, 11);
    entries[idx].name[11] = '\0';
    entries[idx].size = 0;
    entries[idx].firstblock = -1;

    // -----------------------------
    // 5. WRITE BACK fentry TABLE
    // -----------------------------
    rewind(fp);
    fwrite(entries, sizeof(fentry), MAXFILES, fp);
    fflush(fp);

    // -----------------------------
    // 6. CLOSE FILESYSTEM
    // -----------------------------
    closefs(fp);
}