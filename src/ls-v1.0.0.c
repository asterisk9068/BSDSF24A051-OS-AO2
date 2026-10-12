/*
 * Programming Assignment 02: ls-v1.2.0
 * Features: Long listing (-l) + Default column display (down then across)
 * Instructor: Muhammad Arif Butt, PhD
 *
 * Usage:
 *   $ ./bin/ls               # column display (down-then-across)
 *   $ ./bin/ls -l            # long listing
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

/* Flags */
static int long_flag = 0;

/* Forward declarations */
void do_ls(const char *dir);
void do_ls_long(const char *dir);
void print_permissions(mode_t mode);
void print_columns(char **names, int count);
int  get_terminal_width(void);

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "l")) != -1) {
        switch (opt) {
            case 'l':
                long_flag = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        if (long_flag)
            do_ls_long(".");
        else
            do_ls(".");
    } else {
        for (int i = optind; i < argc; i++) {
            if (argc - optind > 1)
                printf("\n%s:\n", argv[i]);
            if (long_flag)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
        }
    }
    return 0;
}

/*
 * get_terminal_width - returns terminal width via ioctl, falls back to 80
 */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80; /* fallback */
}

/*
 * print_columns - prints names in "down then across" columnar format
 *
 * Example with 8 files and 3 columns:
 *   file1   file4   file7
 *   file2   file5   file8
 *   file3   file6
 *
 * We iterate row by row; for row r and column c, the index is: r + c*num_rows
 */
void print_columns(char **names, int count)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();

    /* Find longest name */
    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(names[i]);
        if (len > max_len)
            max_len = len;
    }

    int col_width  = max_len + 2;           /* padding between columns */
    int num_cols   = term_width / col_width;
    if (num_cols < 1)
        num_cols = 1;

    int num_rows = (count + num_cols - 1) / num_cols;  /* ceiling division */

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int idx = row + col * num_rows;
            if (idx >= count)
                break;
            /* Last item in row: no trailing spaces */
            if (col == num_cols - 1 || idx + num_rows >= count)
                printf("%s", names[idx]);
            else
                printf("%-*s", col_width, names[idx]);
        }
        putchar('\n');
    }
}

/*
 * do_ls - default listing: reads all names, then prints in columns
 */
void do_ls(const char *dir)
{
    DIR           *dp;
    struct dirent *entry;
    char         **names    = NULL;
    int            count    = 0;
    int            capacity = 64;

    dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    names = malloc(capacity * sizeof(char *));
    if (!names) {
        perror("malloc");
        closedir(dp);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity) {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (!tmp) {
                perror("realloc");
                break;
            }
            names = tmp;
        }

        names[count] = strdup(entry->d_name);
        if (!names[count]) {
            perror("strdup");
            break;
        }
        count++;
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);

    /* Print in columns (down-then-across) */
    print_columns(names, count);

    /* Free memory */
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
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
 * do_ls_long - long listing format (-l)
 */
void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat   st;
    struct passwd *pw;
    struct group  *gr;
    char          path[4096];
    char          timebuf[64];
    time_t        mtime;

    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

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

        printf(" %s\n", entry->d_name);
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
}
