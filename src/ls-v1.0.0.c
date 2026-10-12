/*
 * Programming Assignment 02: ls-v1.3.0
 * Features: Long listing (-l) + Column display + Horizontal display (-x)
 * Instructor: Muhammad Arif Butt, PhD
 *
 * Usage:
 *   $ ./bin/ls               # column display (down-then-across)
 *   $ ./bin/ls -l            # long listing
 *   $ ./bin/ls -x            # horizontal (across) display
 *   $ ./bin/ls [directory]
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

/* Display mode flags */
#define MODE_DEFAULT    0   /* down-then-across columns */
#define MODE_LONG       1   /* -l long listing */
#define MODE_HORIZONTAL 2   /* -x horizontal (left-to-right) */

static int display_mode = MODE_DEFAULT;

/* Forward declarations */
void do_ls(const char *dir);
void do_ls_long(const char *dir);
void print_permissions(mode_t mode);
void print_columns(char **names, int count);
void print_horizontal(char **names, int count);
int  get_terminal_width(void);
char **read_names(const char *dir, int *count);
void free_names(char **names, int count);
int  compare_str(const void *a, const void *b);

/*
 * compare_str - comparison function for qsort() on arrays of char*
 *
 * qsort passes pointers to elements; since our array holds char*,
 * each element pointer is a char**, so we cast and dereference.
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
    while ((opt = getopt(argc, argv, "lx")) != -1) {
        switch (opt) {
            case 'l':
                display_mode = MODE_LONG;
                break;
            case 'x':
                display_mode = MODE_HORIZONTAL;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l|-x] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        do_ls(".");
    } else {
        for (int i = optind; i < argc; i++) {
            if (argc - optind > 1)
                printf("\n%s:\n", argv[i]);
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
 * read_names - reads non-hidden directory entries into a dynamic array.
 * Caller must free with free_names().
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

    /* Sort alphabetically using qsort */
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
 * print_columns - "down then across" columnar layout
 *
 * Iterates row-by-row. For each row r and column c, index = r + c * num_rows.
 * Example (8 files, 3 cols, 3 rows):
 *   file1   file4   file7
 *   file2   file5   file8
 *   file3   file6
 */
void print_columns(char **names, int count)
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
            if (col == num_cols - 1 || idx + num_rows >= count)
                printf("%s", names[idx]);
            else
                printf("%-*s", col_width, names[idx]);
        }
        putchar('\n');
    }
}

/*
 * print_horizontal - left-to-right (across) layout
 *
 * Prints filenames left-to-right, wrapping when the line would exceed
 * terminal width.  Each entry is padded to col_width so columns align.
 */
void print_horizontal(char **names, int count)
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
    int pos = 0;  /* current horizontal position on line */

    for (int i = 0; i < count; i++) {
        /* Would this item overflow the line? */
        if (pos > 0 && pos + col_width > term_width) {
            putchar('\n');
            pos = 0;
        }
        /* Last item on the line: no trailing pad */
        if (pos + col_width > term_width || i == count - 1)
            printf("%s", names[i]);
        else
            printf("%-*s", col_width, names[i]);
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
 * do_ls_long - long listing format (-l), now uses sorted read_names()
 */
void do_ls_long(const char *dir)
{
    struct stat   st;
    struct passwd *pw;
    struct group  *gr;
    char          path[4096];
    char          timebuf[64];
    time_t        mtime;
    int           count = 0;

    /* Read and sort names using the shared helper */
    char **names = read_names(dir, &count);
    if (!names)
        return;

    for (int i = 0; i < count; i++) {
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
        printf(" %s", timebuf);

        printf(" %s\n", names[i]);
    }

    free_names(names, count);
}


/*
 * do_ls - main dispatcher: reads names then calls appropriate display function
 */
void do_ls(const char *dir)
{
    if (display_mode == MODE_LONG) {
        do_ls_long(dir);
        return;
    }

    int    count = 0;
    char **names = read_names(dir, &count);
    if (!names)
        return;

    if (display_mode == MODE_HORIZONTAL)
        print_horizontal(names, count);
    else
        print_columns(names, count);   /* MODE_DEFAULT */

    free_names(names, count);
}
