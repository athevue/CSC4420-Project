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
    // 1.5 DUPLICATE CHECK (PUT HERE)
    // -----------------------------
    for (int i = 0; i < MAXFILES; i++) {
        if (strcmp(entries[i].name, filename) == 0) {
            fprintf(stderr, "Error: file already exists\n");
            closefs(fp);
            return;
        }
    }

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

void readfile(char *fsname, char *filename, int start, int length)
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

    if (start > entries[idx].size || start + length > entries[idx].size) {
        fprintf(stderr, "Error: invalid read range\n");
        closefs(fp);
        return;
    }

    long data_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;

    int current = entries[idx].firstblock;
    int offset = start;

    while (offset >= BLOCKSIZE) {
        current = nodes[current].nextblock;
        offset -= BLOCKSIZE;
    }

    int bytes_left = length;
    char buffer[BLOCKSIZE];

    while (current != -1 && bytes_left > 0) {
        int block = nodes[current].blockindex;

        fseek(fp, data_start + block * BLOCKSIZE, SEEK_SET);
        fread(buffer, 1, BLOCKSIZE, fp);

        int to_print = BLOCKSIZE - offset;
        if (to_print > bytes_left)
            to_print = bytes_left;

        fwrite(buffer + offset, 1, to_print, stdout);

        bytes_left -= to_print;
        offset = 0;

        current = nodes[current].nextblock;
    }

    closefs(fp);
}

void writefile(char *fsname, char *filename, int start, int length)
{
    FILE *fp = openfs(fsname, "r+b");

    fentry entries[MAXFILES];
    fnode nodes[MAXBLOCKS];

    rewind(fp);
    fread(entries, sizeof(fentry), MAXFILES, fp);
    fread(nodes, sizeof(fnode), MAXBLOCKS, fp);

    // -----------------------------
    // 1. find file
    // -----------------------------
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

    // -----------------------------
    // 2. read input data
    // -----------------------------
    char input[length];
    fread(input, 1, length, stdin);

    // -----------------------------
    // 3. calculate data region start
    // -----------------------------
    long data_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;

    // -----------------------------
    // 4. if file empty → allocate first block
    // -----------------------------
    if (entries[idx].firstblock == -1) {

        // find free fnode
        int node = -1;
        for (int i = 0; i < MAXBLOCKS; i++) {
            if (nodes[i].blockindex < 0) {
                node = i;
                break;
            }
        }

        if (node == -1) {
            fprintf(stderr, "Error: no free blocks\n");
            closefs(fp);
            return;
        }

        // assign block
        nodes[node].blockindex = node;
        nodes[node].nextblock = -1;
        entries[idx].firstblock = node;
    }

    // -----------------------------
    // 5. traverse to correct start position
    // -----------------------------
    int current = entries[idx].firstblock;
    int offset = start;

    while (offset >= BLOCKSIZE) {
        if (nodes[current].nextblock == -1) {
            // allocate new block
            int newnode = -1;

            for (int i = 0; i < MAXBLOCKS; i++) {
                if (nodes[i].blockindex < 0) {
                    newnode = i;
                    break;
                }
            }

            if (newnode == -1) {
                fprintf(stderr, "Error: no free blocks\n");
                closefs(fp);
                return;
            }

            nodes[current].nextblock = newnode;
            nodes[newnode].blockindex = newnode;
            nodes[newnode].nextblock = -1;
        }

        current = nodes[current].nextblock;
        offset -= BLOCKSIZE;
    }

    // -----------------------------
    // 6. write data
    // -----------------------------
    int bytes_written = 0;

    while (bytes_written < length) {

        int block = nodes[current].blockindex;

        char buffer[BLOCKSIZE] = {0};

        fseek(fp, data_start + block * BLOCKSIZE, SEEK_SET);
        fread(buffer, 1, BLOCKSIZE, fp);

        int write_amount = BLOCKSIZE - offset;
        if (write_amount > (length - bytes_written))
            write_amount = length - bytes_written;

        memcpy(buffer + offset, input + bytes_written, write_amount);

        fseek(fp, data_start + block * BLOCKSIZE, SEEK_SET);
        fwrite(buffer, 1, BLOCKSIZE, fp);

        bytes_written += write_amount;
        offset = 0;

        if (bytes_written < length) {

            if (nodes[current].nextblock == -1) {
                int newnode = -1;

                for (int i = 0; i < MAXBLOCKS; i++) {
                    if (nodes[i].blockindex < 0) {
                        newnode = i;
                        break;
                    }
                }

                if (newnode == -1) {
                    fprintf(stderr, "Error: no free blocks\n");
                    closefs(fp);
                    return;
                }

                nodes[current].nextblock = newnode;
                nodes[newnode].blockindex = newnode;
                nodes[newnode].nextblock = -1;
            }

            current = nodes[current].nextblock;
        }
    }

    // -----------------------------
    // 7. update file size
    // -----------------------------
    if (start + length > entries[idx].size) {
        entries[idx].size = start + length;
    }

    // -----------------------------
    // 8. write metadata back
    // -----------------------------
    rewind(fp);
    fwrite(entries, sizeof(fentry), MAXFILES, fp);
    fwrite(nodes, sizeof(fnode), MAXBLOCKS, fp);
    fflush(fp);

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

    int found = 0;

    for (int i = 0; i < MAXFILES; i++) {
        if (strcmp(entries[i].name, filename) == 0) {

            found = 1;

            // free file blocks
            int current = entries[i].firstblock;

            while (current != -1) {
                int next = nodes[current].nextblock;

                nodes[current].blockindex = -1;
                nodes[current].nextblock = -1;

                current = next;
            }

            // clear entry
            memset(entries[i].name, 0, 12);
            entries[i].size = 0;
            entries[i].firstblock = -1;
        }
    }

    if (!found) {
        fprintf(stderr, "Error: file not found\n");
        closefs(fp);
        return;
    }

    rewind(fp);
    fwrite(entries, sizeof(fentry), MAXFILES, fp);
    fwrite(nodes, sizeof(fnode), MAXBLOCKS, fp);

    fflush(fp);
    closefs(fp);
}