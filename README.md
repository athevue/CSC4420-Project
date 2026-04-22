# Simulated File System (simfs)

## Overview
This project is a simplified file system implemented in C. It simulates how an operating system manages files using a single underlying Unix file. The system supports basic file operations such as creating, deleting, reading, and writing files.

The file system contains:
- A fixed number of file entries (directory-like metadata)
- A fixed number of file nodes (used to track data blocks)
- A fixed block size for storing file data

All data is stored inside one file (`myfs.dat`), which acts as the virtual disk.

---

## Features Implemented

### File Operations
- `createfile`  
  Creates a new file in the simulated file system if space is available.

- `deletefile`  
  Removes a file and frees all associated data blocks.

- `writefile`  
  Writes data from standard input into a file starting at a given offset. Allocates new blocks if needed.

- `readfile`  
  Reads data from a file starting at a given offset and prints it to standard output.

- `printfs`  
  Displays the internal structure of the file system (file entries and file nodes).

- `initfs`  
  Initializes a new simulated file system inside the file.

---

## Design Overview

The file system is structured as follows:

- **fentry (file entry):** stores file name, file size, and pointer to first data block.
- **fnode (file node):** tracks individual data blocks and links them together as a chain.
- **Data blocks:** actual file contents stored in fixed-size chunks.

Files are stored using a linked-block system, meaning each file may span multiple non-contiguous blocks.

---

## Error Handling

The program performs strict error checking:
- Prevents file creation if no space is available
- Ensures no invalid file operations are performed
- Avoids modifying the file system in case of an error
- Prints errors to `stderr` and exits safely when needed

---

## How to Build

Compile the project using:

```bash
make
