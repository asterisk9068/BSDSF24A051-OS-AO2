/*
 * Programming Assignment 02: ls-v1.6.0
 * Features: Long listing (-l) + Column display + Horizontal display (-x)
 *           + Alphabetical sort + Colorized output + Recursive listing (-R)
 * Instructor: Muhammad Arif Butt, PhD
 *
 * Usage:
 *   $ ./bin/ls                 # column display (down-then-across, sorted, colored)
 *   $ ./bin/ls -l              # long listing
 *   $ ./bin/ls -x              # horizontal display
 *   $ ./bin/ls -R              # recursive directory traversal
 *   $ ./bin/ls -lR             # long listing recursive
 *   $ ./bin/ls [options] [dir...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

/* ANSI escape codes for coloring */
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_PINK    "\033[0;35m"
#define COLOR_REVERSE "\033[7m"

/* Display mode flags */
#define MODE_DEFAULT    0   /* down-then-across columns */
#define MODE_LONG       1   /* -l long listing */
#define MODE_HORIZONTAL 2   /* -x horizontal (left-to-right) */

static int display_mode   = MODE_DEFAULT;
static int recursive_flag = 0;
static int first_dir      = 1;

/* Forward declarations */
void do_ls(const char *dir);
void do_ls_long(const char *dir, char **names, int count);
void print_permissions(mode_t mode);
void print_columns(const char *dir, char **names, int count);
void print_horizontal(const char *dir, char **names, int count);
void print_colored_name(const char *name, mode_t mode);
int  is_tarball(const char *name);
int  get_terminal_width(void);
char **read_names(const char *dir, int *count);
void free_names(char **names, int count);
int  compare_str(const void *a, const void *b);

/*
 * compare_str - comparison function for qsort() on arrays of char*
 */
int compare_str(const void *a, const void *b)
{
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "lxR")) != -1) {
        switch (opt) {
            case 'l':
                display_mode = MODE_LONG;
                break;
            case 'x':
                display_mode = MODE_HORIZONTAL;
                break;
            case 'R':
                recursive_flag = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l|-x] [-R] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        do_ls(".");
    } else {
        int multiple = (argc - optind > 1);
        for (int i = optind; i < argc; i++) {
            if (!recursive_flag && multiple) {
                if (i > optind)
                    putchar('\n');
                printf("%s:\n", argv[i]);
            }
            do_ls(argv[i]);
        }
    }
    return 0;
}

/*
 * get_terminal_width - returns terminal width via ioctl, fallback to 80
 */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/*
 * is_tarball - returns 1 if filename matches archive extensions
 */
int is_tarball(const char *name)
{
    size_t len = strlen(name);
    if (len >= 4 && strcmp(name + len - 4, ".tar") == 0) return 1;
    if (len >= 3 && strcmp(name + len - 3, ".gz") == 0)  return 1;
    if (len >= 4 && strcmp(name + len - 4, ".zip") == 0) return 1;
    if (len >= 4 && strcmp(name + len - 4, ".tgz") == 0) return 1;
    if (len >= 4 && strcmp(name + len - 4, ".bz2") == 0) return 1;
    if (len >= 3 && strcmp(name + len - 3, ".xz") == 0)  return 1;
    if (strstr(name, ".tar.") != NULL) return 1;
    return 0;
}

/*
 * print_colored_name - prints filename with ANSI color based on file type
 *
 * Rules:
 *   - Symbolic Links: Pink (\033[0;35m)
 *   - Directory: Blue (\033[0;34m)
 *   - Tarballs: Red (\033[0;31m)
 *   - Special Files: Reverse Video (\033[7m)
 *   - Executables: Green (\033[0;32m)
 *   - Regular files: Normal default color
 */
void print_colored_name(const char *name, mode_t mode)
{
    if (S_ISLNK(mode)) {
        printf(COLOR_PINK "%s" COLOR_RESET, name);
    } else if (S_ISDIR(mode)) {
        printf(COLOR_BLUE "%s" COLOR_RESET, name);
    } else if (is_tarball(name)) {
        printf(COLOR_RED "%s" COLOR_RESET, name);
    } else if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode)) {
        printf(COLOR_REVERSE "%s" COLOR_RESET, name);
    } else if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        printf(COLOR_GREEN "%s" COLOR_RESET, name);
    } else {
        printf("%s", name);
    }
}

/*
 * read_names - reads non-hidden directory entries into a dynamic array
 * and sorts them alphabetically with qsort(). Caller frees with free_names().
 */
char **read_names(const char *dir, int *count)
{
    DIR           *dp;
    struct dirent *entry;
    char         **names    = NULL;
    int            n        = 0;
    int            capacity = 64;

    *count = 0;

    dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return NULL;
    }

    names = malloc(capacity * sizeof(char *));
    if (!names) {
        perror("malloc");
        closedir(dp);
        return NULL;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (n == capacity) {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (!tmp) {
                perror("realloc");
                break;
            }
            names = tmp;
        }

        names[n] = strdup(entry->d_name);
        if (!names[n]) {
            perror("strdup");
            break;
        }
        n++;
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);

    /* Sort alphabetically */
    qsort(names, n, sizeof(char *), compare_str);

    *count = n;
    return names;
}

/*
 * free_names - frees array of strings allocated by read_names()
 */
void free_names(char **names, int count)
{
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/*
 * print_columns - "down then across" columnar layout with colorization
 */
void print_columns(const char *dir, char **names, int count)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();

    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(names[i]);
        if (len > max_len)
            max_len = len;
    }

    int col_width = max_len + 2;
    int num_cols  = term_width / col_width;
    if (num_cols < 1)
        num_cols = 1;

    int num_rows = (count + num_cols - 1) / num_cols;

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int idx = row + col * num_rows;
            if (idx >= count)
                break;

            char path[4096];
            if (dir[strlen(dir) - 1] == '/')
                snprintf(path, sizeof(path), "%s%s", dir, names[idx]);
            else
                snprintf(path, sizeof(path), "%s/%s", dir, names[idx]);

            struct stat st;
            if (lstat(path, &st) == -1)
                memset(&st, 0, sizeof(st));

            print_colored_name(names[idx], st.st_mode);

            if (col < num_cols - 1 && idx + num_rows < count) {
                int spaces = col_width - (int)strlen(names[idx]);
                if (spaces > 0)
                    printf("%*s", spaces, "");
            }
        }
        putchar('\n');
    }
}

/*
 * print_horizontal - left-to-right (across) layout with colorization
 */
void print_horizontal(const char *dir, char **names, int count)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();

    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(names[i]);
        if (len > max_len)
            max_len = len;
    }

    int col_width = max_len + 2;
    int pos = 0;

    for (int i = 0; i < count; i++) {
        if (pos > 0 && pos + col_width > term_width) {
            putchar('\n');
            pos = 0;
        }

        char path[4096];
        if (dir[strlen(dir) - 1] == '/')
            snprintf(path, sizeof(path), "%s%s", dir, names[i]);
        else
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

        struct stat st;
        if (lstat(path, &st) == -1)
            memset(&st, 0, sizeof(st));

        print_colored_name(names[i], st.st_mode);

        if (i < count - 1 && pos + col_width <= term_width) {
            int spaces = col_width - (int)strlen(names[i]);
            if (spaces > 0)
                printf("%*s", spaces, "");
        }
        pos += col_width;
    }
    putchar('\n');
}

/*
 * print_permissions - converts st_mode into "drwxrwxrwx" style string
 */
void print_permissions(mode_t mode)
{
    if      (S_ISREG(mode))  putchar('-');
    else if (S_ISDIR(mode))  putchar('d');
    else if (S_ISLNK(mode))  putchar('l');
    else if (S_ISCHR(mode))  putchar('c');
    else if (S_ISBLK(mode))  putchar('b');
    else if (S_ISFIFO(mode)) putchar('p');
    else if (S_ISSOCK(mode)) putchar('s');
    else                     putchar('?');

    putchar((mode & S_IRUSR) ? 'r' : '-');
    putchar((mode & S_IWUSR) ? 'w' : '-');
    putchar((mode & S_ISUID) ? 's' : (mode & S_IXUSR) ? 'x' : '-');
    putchar((mode & S_IRGRP) ? 'r' : '-');
    putchar((mode & S_IWGRP) ? 'w' : '-');
    putchar((mode & S_ISGID) ? 's' : (mode & S_IXGRP) ? 'x' : '-');
    putchar((mode & S_IROTH) ? 'r' : '-');
    putchar((mode & S_IWOTH) ? 'w' : '-');
    putchar((mode & S_ISVTX) ? 't' : (mode & S_IXOTH) ? 'x' : '-');
}

/*
 * do_ls_long - long listing format (-l) with colorization
 */
void do_ls_long(const char *dir, char **names, int count)
{
    struct stat   st;
    struct passwd *pw;
    struct group  *gr;
    char          path[4096];
    char          timebuf[64];
    time_t        mtime;

    for (int i = 0; i < count; i++) {
        if (dir[strlen(dir) - 1] == '/')
            snprintf(path, sizeof(path), "%s%s", dir, names[i]);
        else
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

        if (lstat(path, &st) == -1) {
            perror(path);
            continue;
        }

        print_permissions(st.st_mode);
        printf(" %2lu", (unsigned long)st.st_nlink);

        pw = getpwuid(st.st_uid);
        if (pw)
            printf(" %-8s", pw->pw_name);
        else
            printf(" %-8u", st.st_uid);

        gr = getgrgid(st.st_gid);
        if (gr)
            printf(" %-8s", gr->gr_name);
        else
            printf(" %-8u", st.st_gid);

        printf(" %8lld", (long long)st.st_size);

        mtime = st.st_mtime;
        strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", localtime(&mtime));
        printf(" %s ", timebuf);

        print_colored_name(names[i], st.st_mode);
        putchar('\n');
    }
}

/*
 * do_ls - main directory listing function (recursive when recursive_flag is set)
 */
void do_ls(const char *dir)
{
    if (recursive_flag) {
        if (!first_dir)
            putchar('\n');
        printf("%s:\n", dir);
        first_dir = 0;
    }

    int    count = 0;
    char **names = read_names(dir, &count);
    if (!names)
        return;

    if (display_mode == MODE_LONG)
        do_ls_long(dir, names, count);
    else if (display_mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count);
    else
        print_columns(dir, names, count);   /* MODE_DEFAULT */

    /* If recursive mode is active, descend into subdirectories */
    if (recursive_flag) {
        for (int i = 0; i < count; i++) {
            char subpath[4096];
            if (dir[strlen(dir) - 1] == '/')
                snprintf(subpath, sizeof(subpath), "%s%s", dir, names[i]);
            else
                snprintf(subpath, sizeof(subpath), "%s/%s", dir, names[i]);

            struct stat st;
            if (lstat(subpath, &st) == -1)
                continue;

            /* Check if entry is a directory (and lstat confirms it's not a symlink) */
            if (S_ISDIR(st.st_mode)) {
                do_ls(subpath);
            }
        }
    }

    free_names(names, count);
}
