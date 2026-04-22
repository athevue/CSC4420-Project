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


void deletefile(char *fsname, char *filename)
{
    FILE *fp = openfs(fsname, "r+b");

    fentry entries[MAXFILES];
    fnode nodes[MAXBLOCKS];

    rewind(fp);
    fread(entries, sizeof(fentry), MAXFILES, fp);
    fread(nodes, sizeof(fnode), MAXBLOCKS, fp);

    int idx = -1;
    for (int i = 0; i < MAXFILES; i++) {
        if (strcmp(entries[i].name, filename) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        fprintf(stderr, "Error: file not found\n");
        closefs(fp);
        return;
    }

    long data_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;

    int current = entries[idx].firstblock;

    while (current != -1) {
        int next = nodes[current].nextblock;
        int block = nodes[current].blockindex;

        // zero out data block
        char zeros[BLOCKSIZE] = {0};
        fseek(fp, data_start + block * BLOCKSIZE, SEEK_SET);
        fwrite(zeros, 1, BLOCKSIZE, fp);

        // mark node as unused
        nodes[current].blockindex = -1;
        nodes[current].nextblock = -1;

        current = next;
    }

    // clear file entry
    memset(entries[idx].name, 0, 12);
    entries[idx].size = 0;
    entries[idx].firstblock = -1;

    // write back metadata
    rewind(fp);
    fwrite(entries, sizeof(fentry), MAXFILES, fp);
    fwrite(nodes, sizeof(fnode), MAXBLOCKS, fp);
    fflush(fp);

    closefs(fp);
}