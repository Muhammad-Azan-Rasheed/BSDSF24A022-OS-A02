# BSDSF24A022-OS-A02

A small Unix-style directory listing program developed in C for Operating Systems Programming Assignment 02.

## Build

On Linux or another POSIX environment with a C compiler and `make`:

```sh
make
```

The executable is written to `bin/ls`. Use `make clean` to remove generated files. The `VERSION` setting selects a milestone snapshot: use `make clean && make VERSION=1` through `make VERSION=6`.

## Usage

```sh
./bin/ls [ -l | -x ] [ -R ] [directory ...]
```

- No display option: alphabetically sorted, down-then-across columns.
- `-l`: long listing with permissions, link count, owner, group, size, and modification time.
- `-x`: alphabetically sorted, across-then-down rows.
- `-R`: recursively list subdirectories.

Color is enabled only when standard output is a terminal. Hidden entries are omitted, matching the supplied starter's behavior.

## Project layout

- `src/ls-v1.0.0.c`: assignment implementation, extended from the supplied starter.
- `bin/`: compiled program.
- `obj/`: object files.
- `man/`: reserved for the optional manual page.
- `REPORT.md`: answers to the assignment's analysis questions.
