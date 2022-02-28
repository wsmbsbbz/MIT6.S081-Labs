#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char * fmtname(char *path)
{
    char *p;

    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    return p;
}

void find(char* path, char* name)
{
    int fd;
    char buf[512], *p;
    struct dirent de;
    struct stat st;
    /*printf("path: %s\n", path);*/

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        exit(1);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        exit(1);
        close(fd);
        return;
    }

    switch (st.type) {
        // if st is a file
        case T_FILE:
            p = fmtname(path);
            if (strcmp(p, name) == 0) {
                printf("%s\n", path);
            }
            break;
        case T_DIR:
            strcpy(buf, path);
            /*printf("path: %s, buf: %s\n", path, buf);*/
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                // The next two lines are very important. DO NOT REMOVE!
                if (de.inum == 0)
                    continue;
                if (!(strcmp(de.name, ".") && strcmp(de.name, ".."))) {
                    continue;
                }
                /*printf("de.name: %s, p: %s\n", de.name, p);*/
                memmove(p, de.name, strlen(de.name));
                p[strlen(de.name)] = '\0';
                /*printf("before find buf: %s\n", buf);*/
                find(buf, name);
            }
    }
    close(fd);
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("Usage: find path [path ...]\n");
        exit(1);
    }
    find(argv[1], argv[2]);

    exit(0);
}
