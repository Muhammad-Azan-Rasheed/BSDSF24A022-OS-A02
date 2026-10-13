#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#ifndef LS_VERSION
#define LS_VERSION 6
#endif

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define COLOR_RESET "\033[0m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_RED "\033[1;31m"
#define COLOR_PINK "\033[1;35m"
#define COLOR_REVERSE "\033[7m"
#define COLUMN_GAP 2
#define FALLBACK_WIDTH 80

typedef enum { DISPLAY_VERTICAL, DISPLAY_HORIZONTAL, DISPLAY_LONG } DisplayMode;
typedef struct { char **items; size_t count; size_t capacity; size_t max_name; } NameList;

#if LS_VERSION >= 6
static int recursive_flag;
#endif
static bool color_enabled;

static void free_names(NameList *list) {
    for (size_t i = 0; i < list->count; ++i) free(list->items[i]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int add_name(NameList *list, const char *name) {
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 16;
        char **grown = realloc(list->items, next * sizeof(*grown));
        if (!grown) return -1;
        list->items = grown;
        list->capacity = next;
    }
    list->items[list->count] = strdup(name);
    if (!list->items[list->count]) return -1;
    size_t length = strlen(name);
    if (length > list->max_name) list->max_name = length;
    ++list->count;
    return 0;
}

#if LS_VERSION >= 4
static int compare_names(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}
#endif

#if LS_VERSION >= 2
static int terminal_width(void) {
    struct winsize size;
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0)
        return size.ws_col;
    return FALLBACK_WIDTH;
}
#endif

static char *join_path(const char *directory, const char *name) {
    if (name[0] == '/') return strdup(name);
    size_t dlen = strlen(directory), nlen = strlen(name);
    bool slash = dlen > 0 && directory[dlen - 1] != '/';
    char *path = malloc(dlen + (slash ? 1 : 0) + nlen + 1);
    if (!path) return NULL;
    memcpy(path, directory, dlen);
    size_t offset = dlen;
    if (slash) path[offset++] = '/';
    memcpy(path + offset, name, nlen + 1);
    return path;
}

static const char *file_color(const char *name, const struct stat *info) {
    if (S_ISLNK(info->st_mode)) return COLOR_PINK;
    if (S_ISDIR(info->st_mode)) return COLOR_BLUE;
    if (S_ISREG(info->st_mode) && (info->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) return COLOR_GREEN;
    if (S_ISREG(info->st_mode) && (strstr(name, ".tar") || strstr(name, ".gz") || strstr(name, ".zip"))) return COLOR_RED;
    if (!S_ISREG(info->st_mode)) return COLOR_REVERSE;
    return COLOR_RESET;
}

static void print_name(const char *directory, const char *name) {
    if (!color_enabled) { fputs(name, stdout); return; }
    char *path = join_path(directory, name);
    struct stat info;
    if (path && lstat(path, &info) == 0) {
        const char *color = file_color(name, &info);
        if (strcmp(color, COLOR_RESET) != 0) printf("%s%s%s", color, name, COLOR_RESET);
        else fputs(name, stdout);
    } else fputs(name, stdout);
    free(path);
}

static void mode_string(mode_t mode, char result[11]) {
    result[0] = S_ISREG(mode) ? '-' : S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' :
                S_ISCHR(mode) ? 'c' : S_ISBLK(mode) ? 'b' : S_ISFIFO(mode) ? 'p' : S_ISSOCK(mode) ? 's' : '?';
    result[1] = mode & S_IRUSR ? 'r' : '-'; result[2] = mode & S_IWUSR ? 'w' : '-';
    result[3] = mode & S_IXUSR ? 'x' : '-'; result[4] = mode & S_IRGRP ? 'r' : '-';
    result[5] = mode & S_IWGRP ? 'w' : '-'; result[6] = mode & S_IXGRP ? 'x' : '-';
    result[7] = mode & S_IROTH ? 'r' : '-'; result[8] = mode & S_IWOTH ? 'w' : '-';
    result[9] = mode & S_IXOTH ? 'x' : '-';
    if (mode & S_ISUID) result[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) result[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) result[9] = mode & S_IXOTH ? 't' : 'T';
    result[10] = '\0';
}

static void print_long(const char *directory, const NameList *list) {
    for (size_t i = 0; i < list->count; ++i) {
        char *path = join_path(directory, list->items[i]);
        struct stat info;
        if (!path || lstat(path, &info) < 0) {
            fprintf(stderr, "ls: cannot access '%s': %s\n", list->items[i], strerror(errno));
            free(path); continue;
        }
        char permissions[11], date[32];
        mode_string(info.st_mode, permissions);
        const char *timestamp = ctime(&info.st_mtime);
        if (timestamp) {
            snprintf(date, sizeof(date), "%s", timestamp);
            date[strcspn(date, "\n")] = '\0';
        } else strcpy(date, "??? ??? ?? ??:??:?? ????");
        struct passwd *owner = getpwuid(info.st_uid);
        struct group *group = getgrgid(info.st_gid);
        printf("%s %3lu %-8s %-8s %8lld %s ", permissions, (unsigned long)info.st_nlink,
               owner ? owner->pw_name : "?", group ? group->gr_name : "?",
               (long long)info.st_size, date);
        print_name(directory, list->items[i]);
        putchar('\n');
        free(path);
    }
}

#if LS_VERSION >= 2
static void print_vertical(const char *directory, const NameList *list) {
    if (!list->count) return;
    size_t cell = list->max_name + COLUMN_GAP;
    int width = terminal_width();
    size_t columns = cell ? (size_t)width / cell : 1;
    if (!columns) columns = 1;
    if (columns > list->count) columns = list->count;
    size_t rows = (list->count + columns - 1) / columns;
    for (size_t row = 0; row < rows; ++row) {
        for (size_t col = 0; col < columns; ++col) {
            size_t index = row + col * rows;
            if (index >= list->count) continue;
            print_name(directory, list->items[index]);
            if (col + 1 < columns && row + (col + 1) * rows < list->count) {
                size_t pad = cell - strlen(list->items[index]);
                while (pad--) putchar(' ');
            }
        }
        putchar('\n');
    }
}
#endif

#if LS_VERSION >= 3
static void print_horizontal(const char *directory, const NameList *list) {
    if (!list->count) return;
    size_t cell = list->max_name + COLUMN_GAP;
    size_t width = (size_t)terminal_width();
    size_t used = 0;
    for (size_t i = 0; i < list->count; ++i) {
        size_t length = strlen(list->items[i]);
        if (used && used + cell > width) { putchar('\n'); used = 0; }
        print_name(directory, list->items[i]);
        used += length;
        if (i + 1 < list->count) {
            size_t pad = cell > length ? cell - length : 1;
            while (pad--) putchar(' ');
            used += cell - length;
        }
    }
    putchar('\n');
}
#endif

#if LS_VERSION < 2
static void print_single_column(const char *directory, const NameList *list) {
    for (size_t i = 0; i < list->count; ++i) {
        print_name(directory, list->items[i]);
        putchar('\n');
    }
}
#endif

static int read_names(const char *directory, NameList *list) {
    DIR *dp = opendir(directory);
    if (!dp) { fprintf(stderr, "ls: cannot open directory '%s': %s\n", directory, strerror(errno)); return -1; }
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (add_name(list, entry->d_name) < 0) {
            fprintf(stderr, "ls: out of memory\n"); closedir(dp); return -1;
        }
    }
    if (errno) fprintf(stderr, "ls: error reading '%s': %s\n", directory, strerror(errno));
    closedir(dp);
#if LS_VERSION >= 4
    qsort(list->items, list->count, sizeof(*list->items), compare_names);
#endif
    return 0;
}

static void do_ls(const char *directory, DisplayMode mode, bool show_header) {
    NameList list = {0};
    if (read_names(directory, &list) < 0) return;
    if (show_header) printf("%s:\n", directory);
    if (mode == DISPLAY_LONG) print_long(directory, &list);
#if LS_VERSION >= 3
    else if (mode == DISPLAY_HORIZONTAL) print_horizontal(directory, &list);
#endif
#if LS_VERSION >= 2
    else print_vertical(directory, &list);
#else
    else print_single_column(directory, &list);
#endif

#if LS_VERSION >= 6
    if (recursive_flag) {
        for (size_t i = 0; i < list.count; ++i) {
            char *path = join_path(directory, list.items[i]);
            struct stat info;
            if (path && lstat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
                putchar('\n');
                do_ls(path, mode, true);
            }
            free(path);
        }
    }
#endif
    free_names(&list);
}

int main(int argc, char **argv) {
    DisplayMode mode = DISPLAY_VERTICAL;
    int option;
#if LS_VERSION >= 6
    const char *options = "lxR";
#elif LS_VERSION >= 3
    const char *options = "lx";
#else
    const char *options = "l";
#endif
    while ((option = getopt(argc, argv, options)) != -1) {
        if (option == 'l') mode = DISPLAY_LONG;
#if LS_VERSION >= 3
        else if (option == 'x') mode = DISPLAY_HORIZONTAL;
#endif
#if LS_VERSION >= 6
        else if (option == 'R') recursive_flag = 1;
#endif
        else { fprintf(stderr, "Usage: %s [-l] [-x] [-R] [directory ...]\n", argv[0]); return 2; }
    }
#if LS_VERSION >= 5
    color_enabled = isatty(STDOUT_FILENO);
#else
    color_enabled = false;
#endif
#if LS_VERSION >= 6
    if (optind == argc) { do_ls(".", mode, recursive_flag != 0); return 0; }
#else
    if (optind == argc) { do_ls(".", mode, false); return 0; }
#endif
    for (int i = optind; i < argc; ++i) {
        struct stat info;
        if (stat(argv[i], &info) == 0 && !S_ISDIR(info.st_mode)) {
            NameList one = {0};
            if (add_name(&one, argv[i]) == 0) {
#if LS_VERSION >= 4
                qsort(one.items, one.count, sizeof(*one.items), compare_names);
#endif
                if (mode == DISPLAY_LONG) print_long(".", &one);
                else { print_name(".", argv[i]); putchar('\n'); }
            }
            free_names(&one);
        } else {
#if LS_VERSION >= 6
            if (argc - optind > 1 && !recursive_flag) printf("%s:\n", argv[i]);
            do_ls(argv[i], mode, recursive_flag != 0);
#else
            if (argc - optind > 1) printf("%s:\n", argv[i]);
            do_ls(argv[i], mode, false);
#endif
            if (i + 1 < argc) putchar('\n');
        }
    }
    return 0;
}
