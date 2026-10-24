#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#define DIRSIZ      58
#define T_DIR       1   // ディレクトリ
#define T_FILE      2   // 通常ファイル
#define T_DEV       3   // ブロックデバイス
#define T_CHR       4   // キャラクタデバイス
#define T_SYMLINK   5   // シンボリックリンク
#define T_SOCK      6   // ソケット
#define T_FIFO      7   // FIFO
#define T_FILE_FAT  8   // FAT32ファイル
#define T_DIR_FAT   9   // FAT32ディレクトリ
#define T_UNKNOWN   10

char *fmtname(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;
    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--) ;
    p++;

    // Return blank-padded name.
    //if (strlen(p) >= DIRSIZ)
    //    return p;
    memmove(buf, p, DIRSIZ);
    buf[DIRSIZ] = 0;
    //memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

void format_file_mode(mode_t mode, char *fmod)
{
    mode_t curbit = 0400;

    strcpy(fmod, "-rwxrwxrwx");

    if (S_ISDIR(mode)) fmod[0] = 'd';
    else if (S_ISCHR(mode)) fmod[0] = 'c';
    else if (S_ISBLK(mode)) fmod[0] = 'b';
    else if (S_ISFIFO(mode)) fmod[0] = 'f';
    else if (S_ISLNK(mode)) fmod[0] = 'l';
    else if (S_ISSOCK(mode)) fmod[0] = 's';

    for (char i = 1; i < 10; i++) {
        if (!(mode & curbit))
            fmod[i] = '-';
        curbit >>= 1;
    }

    if (S_ISUID & mode) fmod[3] = 's';
    if (S_ISGID & mode) fmod[6] = 's';
    if (S_ISVTX & mode) fmod[9] = 't';
}

char * fmtuser(uid_t uid, gid_t gid)
{
    if (uid == 0 && gid == 0)
        return "root wheel";
    else if (uid == 1000 && gid == 1000)
        return "zuki staff";
    else
        return "anon anon ";
}

int ls(char *path)
{
    int fd;
    struct dirent *dent;
    struct stat statbuf;
    char filemode[16];
    char filename[64];
    char timestamp[32];
    DIR *dir;

    if (fstatat(AT_FDCWD, path, &statbuf, AT_SYMLINK_NOFOLLOW) < 0) {
        fprintf(stderr, "can't get stat of %s\n", path);
        return (EXIT_FAILURE);
    }

    switch(statbuf.st_dev) {
        case T_DIR:
        case T_DIR_FAT:
            if ((dir = opendir(path)) < 0) {
                perror(path);
                return(EXIT_FAILURE);
            }

            int start = strlen(path) - 1;
            strcpy(filename, path);
            if (filename[start] != '/')
                filename[++start] = '/';
            start++;

            while ((dent = readdir(dir))) {
                strcpy(&filename[start], dent->d_name);
                if (stat(filename, &statbuf) < 0) {
                    printf("Error at stat %s\n", filename);
                    return(EXIT_FAILURE);
                }

                format_file_mode(statbuf.st_mode, filemode);
                strftime(timestamp, 100, "%Y-%m-%d %H:%M:%S", gmtime(&statbuf.st_mtime));
                if (S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode))
                    printf("%s %4ld %s %s %2d, %2d %s\n", filemode, statbuf.st_ino, fmtuser(statbuf.st_uid, statbuf.st_gid), timestamp, (statbuf.st_rdev >> 8) & 0x00ff, statbuf.st_rdev & 0x00ff, dent->d_name);
                else
                    printf("%s %4ld %s %s %6d %s\n", filemode, statbuf.st_ino, fmtuser(statbuf.st_uid, statbuf.st_gid), timestamp, statbuf.st_size, dent->d_name);
            }
            closedir(dir);
            break;
        default:
            format_file_mode(statbuf.st_mode, filemode);
            strftime(timestamp, 100, "%Y-%m-%d %H:%M:%S", gmtime(&statbuf.st_mtime));
            if (S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode))
                printf("%s %4ld %s %s %2d, %2d %s\n", filemode, statbuf.st_ino, fmtuser(statbuf.st_uid, statbuf.st_gid), timestamp, (statbuf.st_rdev >> 8) & 0x00ff, statbuf.st_rdev & 0x00ff, path);
            else
                printf("%s %4ld %s %s %6d %s\n", filemode, statbuf.st_ino, fmtuser(statbuf.st_uid, statbuf.st_gid), timestamp, statbuf.st_size, path);
            break;

    }
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc < 2)
        ret += ls(".");
    else
        for (int i = 1; i < argc; i++)
            ret += ls(argv[i]);
    return ret;
}
