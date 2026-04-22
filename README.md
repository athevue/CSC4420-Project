# simfs – Simulated File System

This project is a simple file system built in C as part of CSC 4420. The idea is to simulate how a real operating system stores and manages files, but everything is contained inside a single file on disk (myfs.dat).

Instead of using the normal file system, this project builds its own way of handling files using custom structures and block management.

---

## What it does

The system supports basic file operations like creating files, deleting them, writing data, and reading it back. There’s also a command that prints out the internal structure of the file system so you can see what’s going on behind the scenes.

The main operations are:

- createfile: adds a new file entry if space is available
- deletefile: removes a file and frees up its blocks
- writefile: writes data into a file starting at a given offset
- readfile: reads data from a file and prints it to the screen
- printfs: shows the current file entries and block layout
- initfs: initializes a fresh file system inside the data file

---

## How it works (rough idea)

The file system keeps two main things:

- file entries, which store file names, sizes, and pointers to where the data starts
- file nodes, which track how data blocks are connected together

Each file is stored in blocks, and those blocks don’t have to be next to each other. Instead, they’re linked together like a chain using the file nodes.

---

## Running it

First, build the project:

```bash
make