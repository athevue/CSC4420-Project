/* This file implements the core file system operations for simfs.
 * It manages file creation, deletion, reading, and writing using
 * fixed-size metadata structures stored inside a single disk file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simfs.h"

/* 
 * Helper: open file safely
 * Ensures program exits if file cannot be accessed.
*/
FILE *openfs(char *filename, char *mode)
{
    FILE *fp;
    if ((fp = fopen(filename, mode)) == NULL) {
        perror("openfs");
        exit(1);
    }
    return fp;
}

/* 
 * Helper: safely close file
*/
void closefs(FILE *fp)
{
    if (fclose(fp) != 0) {
        perror("closefs");
        exit(1);
    }
}

/* 
 * CREATE FILE
 * Adds a new file entry if space is available and name is unique.
*/
void createfile(char *fsname, char *filename)
{
    FILE *fp = openfs(fsname, "r+b");

    fentry entries[MAXFILES];

    /* load current directory (file entries) */
    rewind(fp);
    fread(entries, sizeof(fentry), MAXFILES, fp);

    /* check for duplicate file names */
    for (int i = 0; i < MAXFILES; i++) {
        if (strcmp(entries[i].name, filename) == 0) {
            fprintf(stderr, "Error: file already exists\n");
            closefs(fp);
            return;
        }
    }

    /* find first available slot in directory */
    int idx = -1;
    for (int i = 0; i < MAXFILES; i++) {
        if (entries[i].name[0] == '\0') {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        fprintf(stderr, "Error: no free file entries\n");
        closefs(fp);
        return;
    }

    /* initialize new file entry metadata */
    strncpy(entries[idx].name, filename, 11);
    entries[idx].name[11] = '\0';
    entries[idx].size = 0;
    entries[idx].firstblock = -1;

    /* write updated directory back to disk */
    rewind(fp);
    fwrite(entries, sizeof(fentry), MAXFILES, fp);
    fflush(fp);

    closefs(fp);
}

/* 
 * READ FILE
 * Traverses file blocks and prints requested data range.
*/
void readfile(char *fsname, char *filename, int start, int length)
{
    FILE *fp = openfs(fsname, "r+b");

    fentry entries[MAXFILES];
    fnode nodes[MAXBLOCKS];

    /* load metadata (directory + block list) */
    rewind(fp);
    fread(entries, sizeof(fentry), MAXFILES, fp);
    fread(nodes, sizeof(fnode), MAXBLOCKS, fp);

    /* locate file in directory */
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

    /* ensure read is within file bounds */
    if (start > entries[idx].size || start + length > entries[idx].size) {
        fprintf(stderr, "Error: invalid read range\n");
        closefs(fp);
        return;
    }

    long data_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;

    /* move to correct starting block */
    int current = entries[idx].firstblock;
    int offset = start;

    /* skip full blocks until reaching starting offset */
    while (offset >= BLOCKSIZE) {
        current = nodes[current].nextblock;
        offset -= BLOCKSIZE;
    }

    int bytes_left = length;
    char buffer[BLOCKSIZE];

    /* read across linked blocks */
    while (current != -1 && bytes_left > 0) {

        int block = nodes[current].blockindex;

        fseek(fp, data_start + block * BLOCKSIZE, SEEK_SET);
        fread(buffer, 1, BLOCKSIZE, fp);

        /* calculate how much to output from this block */
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

/* 
 * WRITE FILE
 * Writes data into file, allocating new blocks if needed.
*/
void writefile(char *fsname, char *filename, int start, int length)
{
    FILE *fp = openfs(fsname, "r+b");

    fentry entries[MAXFILES];
    fnode nodes[MAXBLOCKS];

    /* load file system metadata */
    rewind(fp);
    fread(entries, sizeof(fentry), MAXFILES, fp);
    fread(nodes, sizeof(fnode), MAXBLOCKS, fp);

    /* locate file */
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

    /* read input data from stdin */
    char input[length];
    fread(input, 1, length, stdin);

    long data_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;

    /* allocate first block if file is empty */
    if (entries[idx].firstblock == -1) {

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

        nodes[node].blockindex = node;
        nodes[node].nextblock = -1;
        entries[idx].firstblock = node;
    }

    /* navigate to correct starting position */
    int current = entries[idx].firstblock;
    int offset = start;

    while (offset >= BLOCKSIZE) {

        /* allocate new block if needed while traversing */
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
        offset -= BLOCKSIZE;
    }

    int bytes_written = 0;

    /* write data across blocks */
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

        /* allocate next block if more data remains */
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

    /* update file size if expanded */
    if (start + length > entries[idx].size) {
        entries[idx].size = start + length;
    }

    /* write updated metadata */
    rewind(fp);
    fwrite(entries, sizeof(fentry), MAXFILES, fp);
    fwrite(nodes, sizeof(fnode), MAXBLOCKS, fp);
    fflush(fp);

    closefs(fp);
}

/* 
 * DELETE FILE
 * Frees all blocks and removes file entry from directory.
*/
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

            int current = entries[i].firstblock;

            /* free all linked blocks */
            while (current != -1) {
                int next = nodes[current].nextblock;

                nodes[current].blockindex = -1;
                nodes[current].nextblock = -1;

                current = next;
            }

            /* clear file metadata */
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