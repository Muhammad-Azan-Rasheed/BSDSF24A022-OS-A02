# Assignment 02 Report

## Feature 1: Project setup

The project uses `src/` for C source, `obj/` for the compiler object file, `bin/` for the executable, and `man/` for the optional manual page. The Makefile builds `bin/ls` from the supplied starter's source path.

## Feature 2: Long listing (`-l`)

### What is the difference between `stat()` and `lstat()`? When is `lstat()` appropriate for `ls`?

`stat(path, &info)` follows a symbolic link and reports metadata about the link's target. `lstat(path, &info)` reports metadata about the link itself. `ls` should use `lstat()` when it needs to identify and display a symbolic link as a link, including its link type and permissions, rather than silently describing the target. This implementation uses `lstat()` for entries and uses `S_ISLNK()` when choosing a color.

### How do bitwise operators and macros extract type and permissions from `st_mode`?

`st_mode` is a bit field. File-type macros such as `S_ISDIR(mode)`, `S_ISREG(mode)`, and `S_ISLNK(mode)` test the type bits. Permission bits can be tested with expressions such as `(mode & S_IRUSR) != 0` for owner-read and `(mode & S_IXOTH) != 0` for other-execute. The implementation checks the nine read/write/execute bits and also handles set-user-ID, set-group-ID, and sticky bits when constructing the ten-character mode string.

## Feature 3: Vertical columns (`ls`)

### How does down-then-across printing work, and why is one simple loop insufficient?

The program first stores all visible names and finds the longest name. It estimates the number of columns from the terminal width and cell width, then computes the row count. For each row it selects indices `row`, `row + rows`, `row + 2 * rows`, and so on. A single sequential loop would print row-major order, placing each successive filename beside the previous one; that is across-then-down, not the required down-then-across order.

### What is `ioctl` used for, and what are the limits of a fixed 80-column fallback?

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &size)` asks the terminal for its current dimensions, including its column count. If the output is redirected or the query fails, this program uses 80 columns. A fixed fallback cannot adapt to narrow or wide terminals, so it can wrap unexpectedly or leave substantial unused space.

## Feature 4: Horizontal columns (`-x`)

### Compare the complexity of vertical and horizontal layouts.

Down-then-across needs the complete item count before printing because it must determine the row count and calculate column-major indices. Across-then-down can visit names in sorted order and wrap when the next padded cell would exceed the terminal width. It still needs the maximum name length to choose a consistent cell width, but its indexing and row calculation are simpler.

### How are the display modes managed?

`getopt()` recognizes `-l`, `-x`, and `-R`. A `DisplayMode` value selects long, horizontal, or default vertical output. The recursive flag is tracked separately so recursion can be combined with each display mode. After names are read and sorted, `do_ls()` dispatches to the selected printer.

## Feature 5: Alphabetical sorting

### Why must entries be collected before sorting, and what are the drawbacks for huge directories?

Sorting requires the whole set of names to be available to `qsort()`, so directory entries are copied into a dynamically grown array before display. Memory use grows with both the number and total length of names, and sorting takes roughly `O(n log n)` comparisons. Very large directories can therefore require substantial memory and time; allocation can fail, and output cannot begin until collection and sorting finish.

### What is the `qsort()` comparator's purpose and signature?

The comparator tells `qsort()` how to order two array elements. Its signature is `int compare(const void *left, const void *right)`: it returns a negative value, zero, or a positive value when the first item is less than, equal to, or greater than the second. `qsort()` is generic and receives an element size rather than a C type, so it passes untyped `const void *` pointers. The comparator casts each pointer to the array element type (`char *const *`) and compares the pointed-to strings with `strcmp()`.

## Feature 6: Colorized output

### How do ANSI escape codes produce color? Give the green sequence.

The terminal interprets escape sequences embedded in output as formatting commands. Green text can be emitted as `\033[1;32m` + the text + `\033[0m`; the final reset sequence restores normal formatting. This implementation colors directories blue, executable files green, archive-like names red, symbolic links pink, and special files in reverse video. Color is enabled only for terminal output.

### Which `st_mode` bits identify executable files?

The execute bits are `S_IXUSR` (owner), `S_IXGRP` (group), and `S_IXOTH` (others). Testing `mode & (S_IXUSR | S_IXGRP | S_IXOTH)` identifies a file executable by at least one class.

## Feature 7: Recursive listing (`-R`)

### What is the base case, and how does recursion stop?

The recursion proceeds only for entries whose `lstat()` metadata identifies a directory and whose names are real entries; `.`-prefixed entries are omitted by the reader, so `.` and `..` are never recursed into. A directory with no visible child directories naturally returns after printing its own listing. This prevents cycles through the usual parent and self entries.

### Why construct a full path before the recursive call?

The current working directory does not change during recursion. Calling `do_ls("subdir")` would resolve that name relative to the process's original working directory, which may be unrelated to the parent currently being listed. Constructing `parent_dir/subdir` makes each lookup relative to the correct directory and allows nested paths to be traversed reliably.

## Implementation notes

This project is intended for a POSIX/Linux environment. The supplied starter was retained as the source filename and extended with the assignment's requested options and system calls. Build and run it with `make` on Linux. The Makefile's `VERSION` setting selects the milestone behavior: 1 enables long listing, 2 enables vertical columns, 3 adds `-x`, 4 enables sorting, 5 enables color, and 6 enables recursion. Each feature branch defaults to its milestone; final `main` defaults to version 6.
