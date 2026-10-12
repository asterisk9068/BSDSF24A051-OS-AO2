/*
 * Programming Assignment 02: ls-v1.1.0
 * Feature: Long Listing Format (-l option)
 * Instructor: Muhammad Arif Butt, PhD
 *
 * Usage:
 *   $ ./bin/ls
 *   $ ./bin/ls -l
 *   $ ./bin/ls [directory]
 *   $ ./bin/ls -l [directory]
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
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

    /* Remaining args are directories; default to "." */
    if (optind >= argc) {
        if (long_flag)
            do_ls_long(".");
        else
            do_ls(".");
    } else {
        for (int i = optind; i < argc; i++) {
            if (argc - optind > 1)
                printf("\nDirectory listing of %s:\n", argv[i]);
            if (long_flag)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
        }
    }
    return 0;
}

/*
 * print_permissions - converts st_mode into "drwxrwxrwx" style string
 */
void print_permissions(mode_t mode)
{
    /* File type */
    if      (S_ISREG(mode))  putchar('-');
    else if (S_ISDIR(mode))  putchar('d');
    else if (S_ISLNK(mode))  putchar('l');
    else if (S_ISCHR(mode))  putchar('c');
    else if (S_ISBLK(mode))  putchar('b');
    else if (S_ISFIFO(mode)) putchar('p');
    else if (S_ISSOCK(mode)) putchar('s');
    else                     putchar('?');

    /* Owner */
    putchar((mode & S_IRUSR) ? 'r' : '-');
    putchar((mode & S_IWUSR) ? 'w' : '-');
    putchar((mode & S_ISUID) ? 's' : (mode & S_IXUSR) ? 'x' : '-');

    /* Group */
    putchar((mode & S_IRGRP) ? 'r' : '-');
    putchar((mode & S_IWGRP) ? 'w' : '-');
    putchar((mode & S_ISGID) ? 's' : (mode & S_IXGRP) ? 'x' : '-');

    /* Others */
    putchar((mode & S_IROTH) ? 'r' : '-');
    putchar((mode & S_IWOTH) ? 'w' : '-');
    putchar((mode & S_ISVTX) ? 't' : (mode & S_IXOTH) ? 'x' : '-');
}

/*
 * do_ls_long - long listing format (-l), similar to ls -l
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
        /* Skip hidden files */
        if (entry->d_name[0] == '.')
            continue;

        /* Build full path for stat */
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        if (lstat(path, &st) == -1) {
            perror(path);
            continue;
        }

        /* Permissions */
        print_permissions(st.st_mode);

        /* Hard link count */
        printf(" %2lu", (unsigned long)st.st_nlink);

        /* Owner name */
        pw = getpwuid(st.st_uid);
        if (pw)
            printf(" %-8s", pw->pw_name);
        else
            printf(" %-8u", st.st_uid);

        /* Group name */
        gr = getgrgid(st.st_gid);
        if (gr)
            printf(" %-8s", gr->gr_name);
        else
            printf(" %-8u", st.st_gid);

        /* File size */
        printf(" %8lld", (long long)st.st_size);

        /* Modification time - ctime() returns "Day Mon DD HH:MM:SS YYYY\n" */
        mtime = st.st_mtime;
        strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", localtime(&mtime));
        printf(" %s", timebuf);

        /* Filename */
        printf(" %s\n", entry->d_name);
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
}

/*
 * do_ls - simple listing (one file per line), original v1.0.0 behaviour
 */
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
}
